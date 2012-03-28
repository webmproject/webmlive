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
  if (timestamp < 0 || duration < 0) {
    LOG(ERROR) << "AudioBuffer cannot Init with invalid time values.";
    return kInvalidArg;
  }
  if (!ptr_data || data_length <= 0) {
    LOG(ERROR) << "AudioBuffer cannot Init with a NULL or empty buffer.";
    return kInvalidArg;
  }

  // Confirm that |ptr_data| has capacity for at least one complete sample.
  const int32 kMinBufferSize = (config.bits_per_sample * config.channels * 8);
  if (data_length < kMinBufferSize) {
    LOG(ERROR) << "AudioBuffer cannot Init without a complete sample.";
    return kInvalidArg;
  }

  if (config.format_tag == kAudioFormatIeeeFloat) {
    // DeinterleaveSamples
  } else if (config.format_tag == kAudioFormatPcm) {
    // ConvertToIeeeFloat
    // DeinterleaveSamples
  }

  if (config.channels > 2) {
    // ReorderSamples
  }

  return kSuccess;
}

}  // namespace webmlive
