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

}  // namespace webmlive

#endif  // CLIENT_ENCODER_BUFFER_POOL_H_
