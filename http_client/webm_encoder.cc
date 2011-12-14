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

WebmEncoder::WebmEncoder() {
}

WebmEncoder::~WebmEncoder() {
}

// Creates the encoder object and call its |Init| method.
int WebmEncoder::Init(const WebmEncoderConfig& config) {
  ptr_media_source_.reset(new (std::nothrow) MediaSourceImpl());  // NOLINT
  if (!ptr_media_source_) {
    LOG(ERROR) << "cannot construct media source!";
    return kInitFailed;
  }
  return ptr_media_source_->Init(this, config);
}

// Returns result of encoder object's |Run| method.
int WebmEncoder::Run() {
  return ptr_media_source_->Run();
}

// Returns result of encoder object's |Stop| method.
void WebmEncoder::Stop() {
  ptr_media_source_->Stop();
}

// Returns encoded duration in seconds.
double WebmEncoder::encoded_duration() {
  return ptr_media_source_->encoded_duration();
}

// VideoFrameCallbackInterface
int WebmEncoder::OnVideoFrameReceived(VideoFrame* ptr_frame) {
  if (!ptr_frame) {
    LOG(ERROR) << "OnVideoFrameReceived NULL Frame!";
    return VideoFrameCallbackInterface::kInvalidArg;
  }
  return kSuccess;
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
