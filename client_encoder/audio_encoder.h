// Copyright (c) 2012 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef CLIENT_ENCODER_AUDIO_ENCODER_H_
#define CLIENT_ENCODER_AUDIO_ENCODER_H_

#include "boost/scoped_array.hpp"
#include "boost/scoped_ptr.hpp"
#include "client_encoder/basictypes.h"
#include "client_encoder/client_encoder_base.h"

namespace webmlive {

enum AudioFormat {
  kAudioFormatPcm = 1,
  kAudioFormatVorbis = 2,
  kAudioFormatIeeeFloat = 3,
};

// Audio configuration control structure. Values set to 0 mean use default.
// Only |channels|, |sample_rate|, and |bits_per_sample| are user
// configurable.
struct AudioConfig {
  AudioConfig()
      : format_tag(kAudioFormatPcm),
        channels(2),
        bytes_per_second(0),
        sample_rate(44100),
        block_align(0),
        bits_per_sample(16),
        valid_bits_per_sample(0),
        channel_mask(0) {}

  uint16 format_tag;              // Audio format.
  uint16 channels;                // Number of channels.
  uint32 sample_rate;             // Samples per second.
  uint32 bytes_per_second;        // Average bytes per second.
  uint16 block_align;             // Atomic audio unit size in bytes.
  uint16 bits_per_sample;         // Sample container size.
  uint16 valid_bits_per_sample;   // Valid bits in sample container.
  uint32 channel_mask;            // Channels present in audio stream.
};
#endif  // CLIENT_ENCODER_AUDIO_ENCODER_H_
