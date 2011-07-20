// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef HTTP_CLIENT_WEBM_ENCODER_H_
#define HTTP_CLIENT_WEBM_ENCODER_H_

#pragma once

#include <string>

#include "boost/scoped_ptr.hpp"
#include "chromium/base/basictypes.h"
#include "http_client_base.h"

namespace WebmLive {

class WebmEncoderImpl;

// Basic encoder interface class intended to hide platform specific encoder
// implementation details.
class WebmEncoder {
 public:
  enum {
    // Encoder implementation unable to monitor encoder state.
    kEncodeMonitorError = -111,
    // Encoder implementation unable to control encoder.
    kEncodeControlError = -110,
    // Encoder implementation file writing related error.
    kFileWriteError = -109,
    // Encoder implementation WebM muxing related error.
    kWebmMuxerError = -108,
    // Encoder implementation audio encoding related error.
    kAudioEncoderError = -107,
    // Encoder implementation video encoding related error.
    kVideoEncoderError = -106,
    // Invalid argument passed to method.
    kInvalidArg = -105,
    // Operation not implemented.
    kNotImplemented = -104,
    // Unable to find an audio source.
    kNoAudioSource = -103,
    // Unable to find a video source.
    kNoVideoSource = -102,
    // Encoder implementation initialization failed.
    kInitFailed = -101,
    // Cannot run the encoder.
    kRunFailed = -100,
    kSuccess = 0,
  };
  WebmEncoder();
  ~WebmEncoder();
  // Initializes the encoder. Returns |kSuccess| upon success, or one of the
  // above status codes upon failure.
  int Init(const std::string& out_file_name);
  // Initializes the encoder. Returns |kSuccess| upon success, or one of the
  // above status codes upon failure.
  int Init(const std::wstring& out_file_name);
  // Runs the encoder. Returns |kSuccess| when successful, or one of the above
  // status codes upon failure.
  int Run();
  // Stops the encoder.
  void Stop();
  // Returns encoded duration in seconds.
  double encoded_duration();
 private:
  // Encoder object.
  boost::scoped_ptr<WebmEncoderImpl> ptr_encoder_;
  DISALLOW_COPY_AND_ASSIGN(WebmEncoder);
};

}  // WebmLive

#endif  // HTTP_CLIENT_WEBM_ENCODER_H_
