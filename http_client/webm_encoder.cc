// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "http_client_base.h"
#include "webm_encoder.h"

#ifdef _WIN32
#include "webm_encoder_dshow.h"
#endif

#include <cstdio>
#include <sstream>

#include "debug_util.h"

namespace WebmLive {

WebmEncoder::WebmEncoder()
{
}

WebmEncoder::~WebmEncoder()
{
}

int WebmEncoder::Init(std::string out_file_name)
{
  std::wostringstream fname_cnv;
  fname_cnv << out_file_name.c_str();
  return Init(fname_cnv.str());
}

int WebmEncoder::Init(std::wstring out_file_name)
{
  ptr_encoder_.reset(new (std::nothrow) WebmEncoderImpl());
  if (!ptr_encoder_) {
    DBGLOG("ERROR: cannot construct WebmEncoderImpl.");
    return kInitFailed;
  }
  return ptr_encoder_->Init(out_file_name);
}

int WebmEncoder::Run()
{
  return ptr_encoder_->Run();
}

int WebmEncoder::Stop()
{
  return ptr_encoder_->Stop();
}

} // WebmLive
