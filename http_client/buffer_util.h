// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef HTTP_CLIENT_BUFFER_UTIL_H_
#define HTTP_CLIENT_BUFFER_UTIL_H_

#pragma once

#include "http_client_base.h"

#include <vector>

#include "boost/thread/mutex.hpp"
#include "chromium/base/basictypes.h"

namespace WebmLive {

// Simple buffer object with locking facilities for passing data between
// threads.  The general idea here is that one thread, A, calls |Init| to copy
// data into the buffer, and then |Lock| to lock the buffer. Then, another
// thread, B, calls |GetBuffer| to obtain A's data, and calls |Unlock| to
// unlock the buffer after finishing work on the data.
// Note: communication between threads A and B is not handled by the class.
class LockableBuffer {
 public:
  enum {
    // |GetBuffer| called while unlocked.
    kNotLocked = -2,
    // Invalid argument passed to method.
    kInvalidArg = -1,
    kSuccess = 0,
    // Buffer is locked.
    kLocked = 1,
  };
  LockableBuffer();
  ~LockableBuffer();
  // Returns true if the buffer is locked.
  bool IsLocked();
  // Copies data into the buffer. Does nothing and returns |kLocked| if the
  // buffer is already locked.
  int Init(const uint8* const ptr_data, int32 length);
  // Returns pointer to internal buffer.  Does nothing and return |kNotLocked|
  // if called with the buffer unlocked.
  int GetBuffer(uint8** ptr_buffer, int32* ptr_length);
  // Lock the buffer.  Returns |kLocked| if already locked.
  int Lock();
  // Unlock the buffer. Returns |kNotLocked| if buffer already unlocked.
  int Unlock();
 private:
  // Lock status.
  bool locked_;
  // Mutex protecting lock status.
  boost::mutex mutex_;
  // Internal buffer.
  std::vector<uint8> buffer_;
  DISALLOW_COPY_AND_ASSIGN(LockableBuffer);
};
}  // WebmLive

#endif  // HTTP_CLIENT_BUFFER_UTIL_H_
