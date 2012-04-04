// Copyright (c) 2012 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef CLIENT_ENCODER_BUFFER_POOL_H_
#define CLIENT_ENCODER_BUFFER_POOL_H_

#include <queue>

#include "boost/thread/mutex.hpp"
#include "client_encoder/basictypes.h"
#include "client_encoder/client_encoder_base.h"

namespace webmlive {

// Buffer pooling object used to pass data between threads. In order to be
// managed by this class Buffer objects must implement the following methods:
// uint8* buffer() const;
// int Clone(Type*);
// int Swap(Type*);
template <class Type>
class BufferPool {
 public:
  enum {
    // |Init()| called more than once.
    kAlreadyInitialized = -6,
    // |Push| called before |Init|.
    kNoBuffers = -5,
    // No buffer objects waiting in |active_buffers_|.
    kEmpty = -4,
    // No buffer objects available in |inactive_buffers_|.
    kFull = -3,
    kNoMemory = -2,
    kInvalidArg = -1,
    kSuccess = 0,
  };

  static const int32 kDefaultBufferCount = 4;
  BufferPool() : allow_growth_(false) {}
  ~BufferPool();

  // Allocates |kDefaultBufferCount| buffer objects, pushes them into
  // |inactive_buffers_|, and returns |kSuccess|.
  int Init(bool allow_growth);

  // Grabs a buffer object pointer from |inactive_buffers_|, copies the data
  // from |ptr_buffer|, and pushes it into |active_buffers_|. Returns |kSuccess|
  // when able to store the data. Returns |kFull| when |inactive_buffers_| is
  // empty AND |allow_growth_| is false. Avoids copy using |Type::Swap| whenever
  // possible.
  int Commit(Type* ptr_buffer);

  // Grabs a |VideoFrame| from |active_buffers_| and copies it to |ptr_buffer|.
  // Returns |kSuccess| when able to copy the frame. Returns |kEmpty| when
  // |active_buffers_| contains no |VideoFrame|s.
  int Decommit(Type* ptr_buffer);

  // Drops all queued |VideoFrame|s by moving them all from |active_buffers_| to
  // |inactive_buffers_|.
  void Flush();

 private:
  // Moves or copies |ptr_source| to |ptr_target| using |Type::Swap| or
  // |Type::Clone| based on presence of non-NULL buffer pointer in
  // |ptr_target|.
  int Exchange(Type* ptr_source, Type* ptr_target);

  bool allow_growth_;
  boost::mutex mutex_;
  std::queue<Type*> inactive_buffers_;
  std::queue<Type*> active_buffers_;
  WEBMLIVE_DISALLOW_COPY_AND_ASSIGN(BufferPool);
};

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
  Type* const ptr_pool_frame = inactive_buffers_.front();
  if (Exchange(ptr_buffer, ptr_pool_frame)) {
    return kNoMemory;
  }

  // Move the now active buffer object into the active queue.
  inactive_buffers_.pop();
  active_buffers_.push(ptr_pool_frame);
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

  // Copy active frame data to user frame.
  Type* const ptr_active_frame = active_buffers_.front();
  if (Exchange(ptr_active_frame, ptr_buffer)) {
    return kNoMemory;
  }

  // Put the now inactive frame back in the pool.
  active_buffers_.pop();
  inactive_buffers_.push(ptr_active_frame);
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

}  // namespace webmlive

#endif  // CLIENT_ENCODER_BUFFER_POOL_H_
