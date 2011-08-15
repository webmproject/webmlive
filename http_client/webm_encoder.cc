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

// Create the encoder object and call its |Init| method.
int WebmEncoder::Init(const WebmEncoderSettings& settings) {
  ptr_encoder_.reset(new (std::nothrow) WebmEncoderImpl());
  if (!ptr_encoder_) {
    DBGLOG("ERROR: cannot construct WebmEncoderImpl.");
    return kInitFailed;
  }
  return ptr_encoder_->Init(settings);
}

// Return result of encoder object's |Run| method.
int WebmEncoder::Run() {
  return ptr_encoder_->Run();
}

// Return result of encoder object's |Stop| method.
void WebmEncoder::Stop() {
  ptr_encoder_->Stop();
}

// Return encoded duration in seconds.
double WebmEncoder::encoded_duration() {
  return ptr_encoder_->encoded_duration();
}

}  // namespace webmlive
