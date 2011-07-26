// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "buffer_util.h"
#include "debug_util.h"
#include "webm_buffer_parser.h"

namespace webmlive {

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

///////////////////////////////////////////////////////////////////////////////
// WebmChunkBuffer
//

WebmChunkBuffer::WebmChunkBuffer() : chunk_length_(0) {
}

WebmChunkBuffer::~WebmChunkBuffer() {
}

// Checks if a chunk is ready, or attempts to parse some data by calling
// |WebmBufferParser::Parse|.
// When a chunk is ready, or when |WebmBufferParser::Parse| completes one, sets
// |ptr_chunk_length| and returns true.
bool WebmChunkBuffer::ChunkReady(int32* ptr_chunk_length) {
  if (ptr_chunk_length) {
    *ptr_chunk_length = 0;
    if (chunk_length_ > 0) {
      *ptr_chunk_length = chunk_length_;
      return true;
    }
    if (parser_->Parse(buffer_, &chunk_length_) == kSuccess) {
      *ptr_chunk_length = chunk_length_;
      return true;
    }
  }
  return false;
}

// Inserts data from |ptr_data| at the end of |buffer_|.
int WebmChunkBuffer::BufferData(const uint8* const ptr_data, int32 length) {
  if (!ptr_data || length < 1) {
    DBGLOG("invalid arg(s).");
    return kInvalidArg;
  }
  buffer_.insert(buffer_.end(), ptr_data, ptr_data+length);
  DBGLOG("buffer_ size=" << buffer_.size());
  return kSuccess;
}

// Constructs and inits |parser_|.
int WebmChunkBuffer::Init() {
  parser_.reset(new (std::nothrow) WebmBufferParser());
  if (!parser_) {
    DBGLOG("out of memory");
    return kOutOfMemory;
  }
  return parser_->Init();
}

// Copies the buffered chunk data into |ptr_buf|, erases it from |buffer_|, and
// resets |chunk_length_| to 0.  Resetting |chunk_length_| allows parsing to
// resume in |ChunkReady|.
int WebmChunkBuffer::ReadChunk(uint8* ptr_buf, int32 length) {
  if (!ptr_buf) {
    DBGLOG("NULL buffer pointer");
    return kInvalidArg;
  }
  if (length < chunk_length_) {
    DBGLOG("not enough space for chunk");
    return kUserBufferTooSmall;
  }
  memcpy(ptr_buf, &buffer_[0], chunk_length_);
  Buffer::const_iterator erase_end_pos = buffer_.begin() + chunk_length_;
  buffer_.erase(buffer_.begin(), erase_end_pos);
  chunk_length_ = 0;
  return kSuccess;
}

}  // namespace webmlive
