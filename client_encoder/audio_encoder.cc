// Copyright (c) 2012 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "client_encoder/audio_encoder.h"

#include <new>

#include "glog/logging.h"

namespace webmlive {

AudioBuffer::AudioBuffer()
    : timestamp_(0),
      duration_(0),
      buffer_capacity_(0),
      buffer_length_(0) {
}

AudioBuffer::~AudioBuffer() {
}

int AudioBuffer::Init(const AudioConfig& config,
                      int64 timestamp,
                      int64 duration,
                      const uint8* ptr_data,
                      int32 data_length) {
  if (duration < 0) {
    LOG(ERROR) << "AudioBuffer duration cannot be less than 0.";
    return kInvalidArg;
  }
  if (!ptr_data || data_length <= 0) {
    LOG(ERROR) << "AudioBuffer cannot Init with a NULL or empty buffer.";
    return kInvalidArg;
  }
  if (data_length > buffer_capacity_) {
    buffer_.reset(new (std::nothrow) uint8[data_length]);  // NOLINT
    if (!buffer_) {
      LOG(ERROR) << "AudioBuffer Init cannot allocate buffer.";
      return kNoMemory;
    }
    buffer_capacity_ = data_length;
  }
  config_ = config;
  buffer_length_ = data_length;
  timestamp_ = timestamp;
  duration_ = duration;
  memcpy(buffer_.get(), ptr_data, data_length);
  return kSuccess;
}

int AudioBuffer::Clone(AudioBuffer* ptr_buffer) const {
  if (!ptr_buffer) {
    return kInvalidArg;
  }
  return ptr_buffer->Init(config_,
                          timestamp_,
                          duration_,
                          buffer_.get(),
                          buffer_length_);
}

void AudioBuffer::Swap(AudioBuffer* ptr_buffer) {
  CHECK_NOTNULL(buffer_.get());
  CHECK_NOTNULL(ptr_buffer->buffer_.get());

  const AudioConfig temp = config_;
  config_ = ptr_buffer->config_;
  ptr_buffer->config_ = temp;

  int64 temp_time = duration_;
  duration_ = ptr_buffer->duration_;
  ptr_buffer->duration_ = temp_time;

  temp_time = timestamp_;
  timestamp_ = ptr_buffer->timestamp_;
  ptr_buffer->duration_ = temp_time;

  int32 temp_size = buffer_length_;
  buffer_length_ = ptr_buffer->buffer_length_;
  ptr_buffer->buffer_length_ = temp_size;

  temp_size = buffer_capacity_;
  buffer_capacity_ = ptr_buffer->buffer_capacity_;
  ptr_buffer->buffer_capacity_ = temp_size;

  buffer_.swap(ptr_buffer->buffer_);
}

}  // namespace webmlive
