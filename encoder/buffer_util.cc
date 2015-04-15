// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "encoder/buffer_util.h"

#include <mutex>
#include <thread>

#include "glog/logging.h"

namespace webmlive {

bool BufferQueue::EnqueueBuffer(const std::string& id,
                                const uint8* data, int length) {
  Buffer* buffer = new (std::nothrow) Buffer;
  if (!buffer) {
    LOG(ERROR) << "No memory in BufferQueue::EnqueueBuffer";
    return false;
  }
  buffer->id = id;
  buffer->data.insert(buffer->data.begin(), data, data + length);
  std::lock_guard<std::mutex> lock(mutex_);
  buffer_q_.push(buffer);
  return true;
}

BufferQueue::Buffer* BufferQueue::DequeueBuffer() {
  BufferQueue::Buffer* buffer = NULL;
  std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
  if (lock.owns_lock() && !buffer_q_.empty()) {
    buffer = buffer_q_.front();
    buffer_q_.pop();
  }
  return buffer;
}

size_t BufferQueue::GetNumBuffers() {
  std::lock_guard<std::mutex> lock(mutex_);
  return buffer_q_.size();
}

// Attempts to obtain lock on |mutex_|. Returns value of |locked_| if the lock
// is obtained, assumes locked and returns true otherwise.
bool LockableBuffer::IsLocked() {
  std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
  if (lock.owns_lock()) {
    return locked_;
  }
  return true;
}

// Confirms buffer is unlocked via call to |IsLocked|, obtains lock on
// |mutex_|, and copies the user data into |buffer_|.
int LockableBuffer::Init(const uint8* const ptr_data, int length) {
  if (IsLocked()) {
    return kLocked;
  }
  std::lock_guard<std::mutex> lock(mutex_);
  if (!ptr_data || length <= 0) {
    LOG(ERROR) << "invalid arg(s).";
    return kInvalidArg;
  }
  buffer_.clear();
  buffer_.assign(ptr_data, ptr_data + length);
  return kSuccess;
}

// Confirms buffer is locked via call to |IsLocked|, obtains lock on
// |mutex_|, and copies the user data into |buffer_|.
int LockableBuffer::GetBuffer(uint8** ptr_buffer, int* ptr_length) {
  if (!ptr_length) {
    return kInvalidArg;
  }
  if (!IsLocked()) {
    LOG(ERROR) << "buffer not locked!";
    return kNotLocked;
  }
  *ptr_buffer = &buffer_[0];
  *ptr_length = buffer_.size();
  return kSuccess;
}

// Obtains lock on |mutex_| and sets |locked_| to true.
int LockableBuffer::Lock() {
  std::lock_guard<std::mutex> lock(mutex_);
  int status = kSuccess;
  if (locked_) {
    status = kLocked;
  }
  locked_ = true;
  return status;
}

// Obtains lock on |mutex_| and sets |locked_| to false.
int LockableBuffer::Unlock() {
  std::lock_guard<std::mutex> lock(mutex_);
  int status = kSuccess;
  if (!locked_) {
    status = kNotLocked;
    LOG(ERROR) << "buffer was not locked!";
  }
  locked_ = false;
  return status;
}

}  // namespace webmlive
