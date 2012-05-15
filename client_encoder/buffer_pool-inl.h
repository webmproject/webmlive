// Copyright (c) 2012 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef CLIENT_ENCODER_BUFFER_POOL_INL_H_
#define CLIENT_ENCODER_BUFFER_POOL_INL_H_

#include <queue>

#include "boost/thread/mutex.hpp"
#include "client_encoder/basictypes.h"
#include "client_encoder/buffer_pool.h"
#include "client_encoder/client_encoder_base.h"

namespace webmlive {

template <class Type>
inline BufferPool<Type>::~BufferPool() {
  boost::mutex::scoped_lock lock(mutex_);
  while (!inactive_buffers_.empty()) {
    delete inactive_buffers_.front();
    inactive_buffers_.pop();
  }
  while (!active_buffers_.empty()) {
    delete active_buffers_.front();
    active_buffers_.pop();
  }
}

// Obtains lock and populates |inactive_buffers_| with |Type| pointers.
template <class Type>
inline int BufferPool<Type>::Init(bool allow_growth) {
  boost::mutex::scoped_lock lock(mutex_);
  if (!inactive_buffers_.empty() || !active_buffers_.empty()) {
    return kAlreadyInitialized;
  }
  for (int i = 0; i < kDefaultBufferCount; ++i) {
    Type* const ptr_buffer = new (std::nothrow) Type;  // NOLINT
    if (!ptr_buffer) {
      return kNoMemory;
    }
    inactive_buffers_.push(ptr_buffer);
  }
  allow_growth_ = allow_growth;
  return kSuccess;
}

// Obtains lock, copies |ptr_buffer| data into front buffer object from
// |inactive_buffers_|, and moves the filled buffer object into
// |active_buffers_|.
template <class Type>
inline int BufferPool<Type>::Commit(Type* ptr_buffer) {
  if (!ptr_buffer || !ptr_buffer->buffer()) {
    return kInvalidArg;
  }
  boost::mutex::scoped_lock lock(mutex_);
  if (inactive_buffers_.empty()) {
    if (allow_growth_) {
      Type* const ptr_buffer = new (std::nothrow) Type;  // NOLINT
      if (!ptr_buffer) {
        return kNoMemory;
      }
      inactive_buffers_.push(ptr_buffer);
    } else {
      return kFull;
    }
  }

  // Copy user data into front buffer object from |inactive_buffers_|.
  Type* const ptr_pool_buffer = inactive_buffers_.front();
  if (Exchange(ptr_buffer, ptr_pool_buffer)) {
    return kNoMemory;
  }

  // Move the now active buffer object into the active queue.
  inactive_buffers_.pop();
  active_buffers_.push(ptr_pool_buffer);
  return kSuccess;
}

// Obtains lock, copies front buffer object from |active_buffers_| to
// |ptr_buffer|, and moves the consumed buffer object back into
// |inactive_buffers_|.
template <class Type>
inline int BufferPool<Type>::Decommit(Type* ptr_buffer) {
  if (!ptr_buffer) {
    return kInvalidArg;
  }
  boost::mutex::scoped_lock lock(mutex_);
  if (active_buffers_.empty()) {
    return kEmpty;
  }

  // Put active buffer data in user buffer.
  Type* const ptr_active_buffer = active_buffers_.front();
  if (Exchange(ptr_active_buffer, ptr_buffer)) {
    return kNoMemory;
  }

  // Put the now inactive buffer back in the pool.
  active_buffers_.pop();
  inactive_buffers_.push(ptr_active_buffer);
  return kSuccess;
}

template <class Type>
inline void BufferPool<Type>::Flush() {
  boost::mutex::scoped_lock lock(mutex_);
  while (!active_buffers_.empty()) {
    inactive_buffers_.push(active_buffers_.front());
    active_buffers_.pop();
  }
}

template <class Type>
inline int BufferPool<Type>::Exchange(Type* ptr_source, Type* ptr_target) {
  if (!ptr_source || !ptr_target) {
    return kInvalidArg;
  }
  if (ptr_target->buffer()) {
    ptr_target->Swap(ptr_source);
  } else {
    const int32 status = ptr_source->Clone(ptr_target);
    if (status) {
      return kNoMemory;
    }
  }
  return kSuccess;
}

template <class Type>
inline int BufferPool<Type>::ActiveBufferTimestamp(int64* ptr_timestamp) {
  if (!ptr_timestamp) {
    return kInvalidArg;
  }
  int status = kEmpty;
  boost::mutex::scoped_lock lock(mutex_);
  if (!active_buffers_.empty()) {
    *ptr_timestamp = active_buffers_.front()->timestamp();
    status = kSuccess;
  }
  return status;
}

template <class Type>
inline void BufferPool<Type>::DropActiveBuffer() {
  boost::mutex::scoped_lock lock(mutex_);
  if (!active_buffers_.empty()) {
    inactive_buffers_.push(active_buffers_.front());
    active_buffers_.pop();
  }
}

template <class Type>
inline bool BufferPool<Type>::IsEmpty() const {
  boost::mutex::scoped_lock lock(mutex_);
  return active_buffers_.empty();
}

}  // namespace webmlive

#endif  // CLIENT_ENCODER_BUFFER_POOL_INL_H_
