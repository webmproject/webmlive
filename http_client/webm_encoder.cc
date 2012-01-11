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

// Constructs media source object and calls its |Init| method.
int WebmEncoder::Init(const WebmEncoderConfig& config) {
  ptr_media_source_.reset(new (std::nothrow) MediaSourceImpl());  // NOLINT
  if (!ptr_media_source_) {
    LOG(ERROR) << "cannot construct media source!";
    return kInitFailed;
  }
  if (video_queue_.Init()) {
    LOG(ERROR) << "VideoFrameQueue Init failed!";
    return kInitFailed;
  }
  config_ = config;
  int status = ptr_media_source_->Init(config_, this);
  if (status) {
    LOG(ERROR) << "media source Init failed " << status;
    return kInitFailed;
  }
  config_.actual_video_config = ptr_media_source_->actual_video_config();
  status = video_encoder_.Init(config_);
  if (status) {
    LOG(ERROR) << "video encoder Init failed " << status;
    return kInitFailed;
  }
  return kSuccess;
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
  int status = video_queue_.Commit(ptr_frame);
  if (status) {
    if (status != VideoFrameQueue::kFull) {
      LOG(ERROR) << "VideoFrameQueue Push failed! " << status;
    }
    return kVideoFrameDropped;
  }
  LOG(INFO) << "OnVideoFrameReceived pushed frame.";
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

      // Read a frame for |video_queue_|. Sets |got_frame| to true if a frame
      // is available.
      bool got_frame = false;
      status = video_queue_.Read(&raw_frame_);
      if (status) {
        if (status != VideoFrameQueue::kEmpty) {
          // Really an error; not just an empty queue.
          LOG(ERROR) << "VideoFrameQueue Pop failed! " << status;
          break;
        } else {
          VLOG(4) << "No frames in VideoFrameQueue";
        }
      } else {
        LOG(INFO) << "Encoder thread read raw frame.";
        got_frame = true;
      }

      // Encode a video frame if the |video_queue_| Read was successful.
      if (got_frame) {
        status = video_encoder_.EncodeFrame(raw_frame_, &vp8_frame_);
        if (status) {
          LOG(ERROR) << "Video frame encode failed " << status;
          break;
        }
      }
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

  // Use the configuration ctors to default the requested capture
  // configurations....
  c.requested_audio_config = WebmEncoderConfig::AudioCaptureConfig();
  c.requested_video_config = WebmEncoderConfig::VideoCaptureConfig();

  // And 0 the actual configurations.
  c.actual_audio_config.manual_config = false;
  c.actual_audio_config.channels = 0;
  c.actual_audio_config.sample_rate = 0;
  c.actual_audio_config.sample_size = 0;
  c.actual_video_config.manual_config = false;
  c.actual_video_config.width = 0;
  c.actual_video_config.height = 0;
  c.actual_video_config.frame_rate = 0;

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

