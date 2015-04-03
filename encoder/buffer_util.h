// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef WEBMLIVE_ENCODER_BUFFER_UTIL_H_
#define WEBMLIVE_ENCODER_BUFFER_UTIL_H_

#include <memory>
#include <mutex>
#include <queue>
#include <vector>

#include "encoder/basictypes.h"
#include "encoder/encoder_base.h"

namespace webmlive {

// Thread safe buffer queue. Allows unbounded growth of internal queue.
class BufferQueue {
 public:
  struct Buffer {
    std::string id;
    std::vector<uint8> data;
  };

  BufferQueue() {}
  ~BufferQueue() {}

  // Copies |data| into a new |Buffer| and assigns |id|. Blocks while waiting to
  // obtain lock on |mutex_|. Returns true when |data| is successfully enqueued.
  bool EnqueueBuffer(const std::string& id, const uint8* data, int length);

  // Returns a buffer if one is available. Does not block waiting on |mutex_|;
  // gives up and returns NULL when unable to obtain lock. Non-NULL |Buffer|
  // pointer memory is owned by caller.
  Buffer* DequeueBuffer();

 private:
  bool locked_;
  std::mutex mutex_;
  std::queue<Buffer*> buffer_q_;
};

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
  LockableBuffer() : locked_(false) {}
  ~LockableBuffer() {}
  // Returns true if the buffer is locked.
  bool IsLocked();
  // Copies data into the buffer. Does nothing and returns |kLocked| if the
  // buffer is already locked.
  int Init(const uint8* const ptr_data, int length);
  // Returns pointer to internal buffer.  Does nothing and returns |kNotLocked|
  // if called with the buffer unlocked.
  int GetBuffer(uint8** ptr_buffer, int* ptr_length);
  // Lock the buffer.  Returns |kLocked| if already locked.
  int Lock();
  // Unlock the buffer. Returns |kNotLocked| if buffer already unlocked.
  int Unlock();

 private:
  // Lock status.
  bool locked_;
  // Mutex protecting lock status.
  std::mutex mutex_;
  // Internal buffer.
  std::vector<uint8> buffer_;
  WEBMLIVE_DISALLOW_COPY_AND_ASSIGN(LockableBuffer);
};

}  // namespace webmlive

#endif  // WEBMLIVE_ENCODER_BUFFER_UTIL_H_
