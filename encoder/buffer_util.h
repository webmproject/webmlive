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
  void EnqueueBuffer(const std::string& id, const uint8* data, int length);

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
  int Init(const uint8* const ptr_data, int32 length);
  // Returns pointer to internal buffer.  Does nothing and returns |kNotLocked|
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
  std::mutex mutex_;
  // Internal buffer.
  std::vector<uint8> buffer_;
  WEBMLIVE_DISALLOW_COPY_AND_ASSIGN(LockableBuffer);
};

// Class for buffering unparsed WebM data that provides users with access to
// complete WebM "chunks" for consumption of data in manageable bits. Stores
// unparsed WebM data in a vector until a "chunk" is ready for consumption.
//
// A chunk in this context is one of two things:
// * The first time |ChunkReady| returns true, the chunk is made up of the
//   EBML header, segment info, and segment tracks elements.
// * All subsequent chunks are complete clusters.
class WebmBufferParser;  // Forward declare |WebmChunkBuffer|'s parser object.
class WebmChunkBuffer {
 public:
  enum {
    kUserBufferTooSmall = -3,
    kOutOfMemory = -2,
    kInvalidArg = -1,
    kSuccess = 0,
    kChunkReady = 1,
  };
  WebmChunkBuffer();
  ~WebmChunkBuffer();
  // Checks for a complete "chunk" by attempting to parse buffered data.
  // Returns true and sets |ptr_chunk_length| if one is ready. Always returns
  // false if |ptr_chunk_length| is NULL.
  bool ChunkReady(int32* ptr_chunk_length);
  // Adds data to |buffer_| and returns |kSuccess|.
  int BufferData(const uint8* const ptr_data, int32 length);
  // Moves "chunk" data into your buffer. The data has been from removed from
  // |buffer_| when |kSuccess| is returned.  Returns |kUserBufferTooSmall| if
  // |length| is less than |chunk_length|.
  int ReadChunk(uint8* ptr_buf, int32 length);
  // Initializes |parser_| and returns |kSuccess|.
  int Init();
  // Returns the length of the currently parsed and buffered chunk, or 0 if
  // a complete chunk is not buffered.
  int32 chunk_length() const { return chunk_length_; }

 private:
  typedef std::vector<uint8> Buffer;
  // WebM data parser.
  std::unique_ptr<WebmBufferParser> parser_;
  // Length of the buffered chunk, or 0 if one is not buffered.
  int32 chunk_length_;
  // Data buffer.
  Buffer buffer_;
  WEBMLIVE_DISALLOW_COPY_AND_ASSIGN(WebmChunkBuffer);
};

}  // namespace webmlive

#endif  // WEBMLIVE_ENCODER_BUFFER_UTIL_H_
