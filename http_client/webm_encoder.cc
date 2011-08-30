// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "webm_encoder.h"

#include <cstdio>
#include <sstream>

#include "debug_util.h"
#ifdef _WIN32
#include "win/webm_encoder_dshow.h"
#endif

namespace webmlive {

WebmEncoder::WebmEncoder() {
}

WebmEncoder::~WebmEncoder() {
}

// Creates the encoder object and call its |Init| method.
int WebmEncoder::Init(const WebmEncoderConfig& config) {
  ptr_encoder_.reset(new (std::nothrow) WebmEncoderImpl());
  if (!ptr_encoder_) {
    DBGLOG("ERROR: cannot construct WebmEncoderImpl.");
    return kInitFailed;
  }
  return ptr_encoder_->Init(config);
}

// Returns result of encoder object's |Run| method.
int WebmEncoder::Run() {
  return ptr_encoder_->Run();
}

// Returns result of encoder object's |Stop| method.
void WebmEncoder::Stop() {
  ptr_encoder_->Stop();
}

// Returns encoded duration in seconds.
double WebmEncoder::encoded_duration() {
  return ptr_encoder_->encoded_duration();
}

// Returns default |WebmEncoderConfig|.
WebmEncoderConfig WebmEncoder::DefaultConfig() {
  WebmEncoderConfig c;
  c.vpx_config.bitrate = kDefaultVpxBitrate;
  c.vpx_config.min_quantizer = kDefaultVpxMinQ;
  c.vpx_config.max_quantizer = kDefaultVpxMaxQ;
  c.vpx_config.keyframe_interval = kDefaultVpxKeyframeInterval;
  c.vpx_config.speed = kDefaultVpxSpeed;
  c.vpx_config.static_threshold = kDefaultVpxStaticThreshold;
  c.vpx_config.thread_count = kDefaultVpxThreadCount;
  c.vpx_config.undershoot = kDefaultVpxUndershoot;
  return c;
}

}  // namespace webmlive
