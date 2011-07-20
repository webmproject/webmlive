// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "buffer_util.h"
#include "debug_util.h"

namespace WebmLive {

LockableBuffer::LockableBuffer() : locked_(false) {
}

LockableBuffer::~LockableBuffer() {
}

// Attempts to obtain lock on |mutex_|. Returns value of |locked_| if the lock
// is obtained, assumes locked and returns true otherwise.
bool LockableBuffer::IsLocked() {
  boost::mutex::scoped_try_lock lock(mutex_);
  if (lock.owns_lock()) {
    return locked_;
  }
  return true;
}

// Confirms buffer is unlocked via call to |IsLocked|, obtains lock on
// |mutex_|, and copies the user data into |buffer_|.
int LockableBuffer::Init(const uint8* const ptr_data, int32 length) {
  if (IsLocked()) {
    return kLocked;
  }
  boost::mutex::scoped_lock lock(mutex_);
  if (!ptr_data || length <= 0) {
    DBGLOG("invalid arg(s).");
    return kInvalidArg;
  }
  buffer_.clear();
  buffer_.assign(ptr_data, ptr_data + length);
  return kSuccess;
}

// Confirms buffer is locked via call to |IsLocked|, obtains lock on
// |mutex_|, and copies the user data into |buffer_|.
int LockableBuffer::GetBuffer(uint8** ptr_buffer, int32* ptr_length) {
  if (!ptr_length) {
    return kInvalidArg;
  }
  if (!IsLocked()) {
    DBGLOG("error, buffer not locked!");
    return kNotLocked;
  }
  *ptr_buffer = &buffer_[0];
  *ptr_length = buffer_.size();
  return kSuccess;
}

// Obtains lock on |mutex_| and sets |locked_| to true.
int LockableBuffer::Lock() {
  boost::mutex::scoped_lock lock(mutex_);
  int status = kSuccess;
  if (locked_) {
    status = kLocked;
  }
  locked_ = true;
  return status;
}

// Obtains lock on |mutex_| and sets |locked_| to false.
int LockableBuffer::Unlock() {
  boost::mutex::scoped_lock lock(mutex_);
  int status = kSuccess;
  if (!locked_) {
    status = kNotLocked;
    DBGLOG("buffer was not locked!");
  }
  locked_ = false;
  return status;
}

}  // namespace WebmLive
