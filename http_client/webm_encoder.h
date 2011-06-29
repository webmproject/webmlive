// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef WEBMLIVE_WEBM_ENCODER_H
#define WEBMLIVE_WEBM_ENCODER_H

#pragma once

#include "boost/scoped_ptr.hpp"
#include "chromium/base/basictypes.h"

namespace WebmLive {

class WebmEncoderImpl;

class WebmEncoder {
public:
  enum {
    kAudioEncoderError = -107,
    kVideoEncoderError = -107,
    kInvalidArg = -106,
    kNotImplemented = -105,
    kNoAudioSource = -104,
    kNoVideoSource = -103,
    kInitFailed = -102,
    kRunFailed = -100,
    kSuccess = 0,
  };
  WebmEncoder();
  ~WebmEncoder();
  int Init(std::string out_file_name);
  int Init(std::wstring out_file_name);
  int Run();
  int Stop();
private:
  boost::scoped_ptr<WebmEncoderImpl> ptr_encoder_;
  DISALLOW_COPY_AND_ASSIGN(WebmEncoder);
};

} // WebmLive

#endif // WEBMLIVE_WEBM_ENCODER_H
