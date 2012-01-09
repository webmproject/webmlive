// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "http_client/webm_encoder.h"

#include <cstdio>
#include <sstream>

#include "glog/logging.h"
#ifdef _WIN32
#include "http_client/win/media_source_dshow.h"
#endif

namespace webmlive {

WebmEncoder::WebmEncoder() : stop_(false) {
}

WebmEncoder::~WebmEncoder() {
}

// Creates the encoder object and calls its |Init| method.
int WebmEncoder::Init(const WebmEncoderConfig& config) {
  ptr_media_source_.reset(new (std::nothrow) MediaSourceImpl());  // NOLINT
  if (!ptr_media_source_) {
    LOG(ERROR) << "cannot construct media source!";
    return kInitFailed;
  }
  return ptr_media_source_->Init(config, this);
}

int WebmEncoder::Run() {
  if (encode_thread_) {
    LOG(ERROR) << "non-null encode thread. Already running?";
    return kRunFailed;
  }
  using boost::bind;
  using boost::shared_ptr;
  using boost::thread;
  using std::nothrow;
  encode_thread_ = shared_ptr<thread>(
      new (nothrow) thread(bind(&WebmEncoder::EncoderThread,  // NOLINT
                                this)));
  return kSuccess;
}

// Sets |stop_| to true and calls join on |encode_thread_| to wait for
// |EncoderThread| to finish.
void WebmEncoder::Stop() {
  assert(encode_thread_);
  boost::mutex::scoped_lock lock(mutex_);
  stop_ = true;
  lock.unlock();
  encode_thread_->join();
}

// Returns encoded duration in seconds.
double WebmEncoder::encoded_duration() {
  return 0.0;
}

// VideoFrameCallbackInterface
int WebmEncoder::OnVideoFrameReceived(VideoFrame* ptr_frame) {
  if (!ptr_frame) {
    LOG(ERROR) << "OnVideoFrameReceived NULL Frame!";
    return VideoFrameCallbackInterface::kInvalidArg;
  }
  return kSuccess;
}

// Tries to obtain lock on |mutex_| and returns value of |stop_| if lock is
// obtained. Assumes no stop requested and returns false if unable to obtain
// the lock.
bool WebmEncoder::StopRequested() {
  bool stop_requested = false;
  boost::mutex::scoped_try_lock lock(mutex_);
  if (lock.owns_lock()) {
    stop_requested = stop_;
  }
  return stop_requested;
}

void WebmEncoder::EncoderThread() {
  LOG(INFO) << "EncoderThread started.";
  int status = ptr_media_source_->Run();
  if (status) {
    // media source Run failed; fatal
    LOG(ERROR) << "Unable to run the media source! " << status;
  } else {
    for (;;) {
      if (StopRequested()) {
        LOG(INFO) << "StopRequested returned true, stopping...";
        break;
      }
      status = ptr_media_source_->CheckStatus();
      if (status) {
        LOG(ERROR) << "Media source in bad state, stopping... " << status;
        break;
      }
      // TODO(tomfinegan): This is just a placeholder thats intended to keep
      //                   this tight loop from spinning like mad until the
      //                   vp8 encoder integration is complete
      SwitchToThread();
    }
    ptr_media_source_->Stop();
  }
  LOG(INFO) << "EncoderThread finished.";
}

///////////////////////////////////////////////////////////////////////////////
// WebmEncoderConfig
//

// Returns default |WebmEncoderConfig|.
WebmEncoderConfig WebmEncoder::DefaultConfig() {
  WebmEncoderConfig c;
  c.audio_config.manual_config = false;
  c.audio_config.channels = kDefaultAudioChannels;
  c.audio_config.sample_rate = kDefaultAudioSampleRate;
  c.audio_config.sample_size = kDefaultAudioSampleSize;
  c.video_config.manual_config = false;
  c.video_config.width = kDefaultVideoWidth;
  c.video_config.height = kDefaultVideoHeight;
  c.video_config.frame_rate = kDefaultVideoFrameRate;
  c.vorbis_bitrate = kDefaultVorbisBitrate;
  c.vpx_config.bitrate = kDefaultVpxBitrate;
  c.vpx_config.decimate = kDefaultVpxDecimate;
  c.vpx_config.min_quantizer = kDefaultVpxMinQ;
  c.vpx_config.max_quantizer = kDefaultVpxMaxQ;
  c.vpx_config.keyframe_interval = kDefaultVpxKeyframeInterval;
  c.vpx_config.speed = kDefaultVpxSpeed;
  c.vpx_config.static_threshold = kDefaultVpxStaticThreshold;
  c.vpx_config.thread_count = kDefaultVpxThreadCount;
  c.vpx_config.token_partitions = kDefaultVpxTokenPartitions;
  c.vpx_config.undershoot = kDefaultVpxUndershoot;
  return c;
}

}  // namespace webmlive

