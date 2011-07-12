// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef WEBMLIVE_HTTP_CLIENT_BUFFER_UTIL_H
#define WEBMLIVE_HTTP_CLIENT_BUFFER_UTIL_H

#pragma once

#include "http_client_base.h"

#include <vector>

#include "boost/thread/mutex.hpp"
#include "chromium/base/basictypes.h"

namespace WebmLive {

class LockableBuffer {
 public:
  enum {
    kNotLocked = -2,
    kInvalidArg = -1,
    kSuccess = 0,
    kLocked = 1,
  };
  LockableBuffer();
  ~LockableBuffer();
  bool IsLocked();
  int Init(const uint8* const ptr_data, int32 length);
  int GetBuffer(uint8** ptr_buffer, int32* ptr_length);
  int Lock();
  int Unlock();
 private:
  bool locked_;
  boost::mutex mutex_;
  std::vector<uint8> buffer_;
  DISALLOW_COPY_AND_ASSIGN(LockableBuffer);
};

} // WebmLive

#endif // WEBMLIVE_HTTP_CLIENT_BUFFER_UTIL_H
