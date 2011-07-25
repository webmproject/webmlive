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
#include "webm_encoder_dshow.h"
#endif

namespace webmlive {

WebmEncoder::WebmEncoder() {
}

WebmEncoder::~WebmEncoder() {
}

// Convert |out_file_name| to a |std::wstring| and call |Init|.
int WebmEncoder::Init(const std::string& out_file_name) {
  std::wostringstream fname_cnv;
  fname_cnv << out_file_name.c_str();
  return Init(fname_cnv.str());
}

// Create the encoder object and call its |Init| method.
int WebmEncoder::Init(const std::wstring& out_file_name) {
  ptr_encoder_.reset(new (std::nothrow) WebmEncoderImpl());
  if (!ptr_encoder_) {
    DBGLOG("ERROR: cannot construct WebmEncoderImpl.");
    return kInitFailed;
  }
  return ptr_encoder_->Init(out_file_name);
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
