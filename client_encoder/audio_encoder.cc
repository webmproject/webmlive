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

  // Determine buffer size required to store input samples, and reallocate
  // |buffer_| if necessary.
  config_ = config;
  int required_buf_size = 0;
  if (config.format_tag == kAudioFormatPcm) {
    // Calculate buffer size for IEEE floating point samples.
    const int bytes_per_input_sample = config.bits_per_sample + 7 / 8;
    const int num_samples =
        (data_length + (bytes_per_input_sample - 1)) / bytes_per_input_sample;
    const int bytes_per_ieee_sample = 4;
    required_buf_size = bytes_per_ieee_sample * num_samples;

    config_.format_tag = kAudioFormatIeeeFloat;
    config_.bits_per_sample = 32;
    config_.block_align = bytes_per_ieee_sample * config_.channels;
    config_.bytes_per_second = config_.sample_rate * config_.block_align;
    config_.valid_bits_per_sample = config_.bits_per_sample;
  } else {
    required_buf_size = data_length;
  }

  if (required_buf_size > buffer_capacity_) {
    buffer_.reset(new (std::nothrow) uint8[required_buf_size]);  // NOLINT
    if (!buffer_) {
      LOG(ERROR) << "AudioBuffer Init cannot allocate buffer.";
      return kNoMemory;
    }
    buffer_capacity_ = required_buf_size;
  }

  // PCM input samples must be converted to IEEE floating point samples and
  // deinterleaved.

  // IEEE floating point input samples must be deinterleaved.

  // Vorbis samples must be left alone.

  if (config.channels > 2) {
    // ReorderSamples
  }

  return kSuccess;
}

}  // namespace webmlive

#if 0
    // Some code and comments borrowed from the media foundation vorbis decoder
    // component.

    // On channel ordering, from the vorbis spec:
    // http://xiph.org/vorbis/doc/Vorbis_I_spec.html#x1-800004.3.9
    // one channel
    //   the stream is monophonic
    // two channels
    //   the stream is stereo. channel order: left, right
    // three channels
    //   the stream is a 1d-surround encoding. channel order: left, center,
    //   right
    // four channels
    //   the stream is quadraphonic surround. channel order: front left, front
    //   right, rear left, rear right
    // five channels
    //   the stream is five-channel surround. channel order: front left,
    //   center, front right, rear left, rear right
    // six channels
    //   the stream is 5.1 surround. channel order: front left, center,
    //   front right, rear left, rear right, LFE
    // seven channels
    //   the stream is 6.1 surround. channel order: front left, center,
    //   front right, side left, side right, rear center, LFE
    // eight channels
    //   the stream is 7.1 surround. channel order: front left, center,
    //   front right, side left, side right, rear left, rear right, LFE
    // greater than eight channels
    //   channel use and order is defined by the application

    switch (vorbis_channels)
    {
        case 3:
            m_output_samples.push_back(ptr_blocks[0][sample]); // FL
            m_output_samples.push_back(ptr_blocks[2][sample]); // FR
            m_output_samples.push_back(ptr_blocks[1][sample]); // FC
            break;
        case 5:
            m_output_samples.push_back(ptr_blocks[0][sample]); // FL
            m_output_samples.push_back(ptr_blocks[2][sample]); // FR
            m_output_samples.push_back(ptr_blocks[1][sample]); // FC
            m_output_samples.push_back(ptr_blocks[3][sample]); // BL
            m_output_samples.push_back(ptr_blocks[4][sample]); // BR
            break;
        case 6:
            // WebM Vorbis decode multi-channel ordering
            // 5.1 Vorbis to PCM (Decoding)
            // Vorbis                PCM
            // 0 Front Left   => 0 Front Left
            // 1 Front Center => 2 Front Right
            // 2 Front Right  => 1 Front Center
            // 3 Back Left    => 5 LFE
            // 4 Back Right   => 3 Back Left
            // 5 LFE          => 4 Back Right
            m_output_samples.push_back(ptr_blocks[0][sample]); // FL
            m_output_samples.push_back(ptr_blocks[2][sample]); // FR
            m_output_samples.push_back(ptr_blocks[1][sample]); // FC
            m_output_samples.push_back(ptr_blocks[5][sample]); // LFE
            m_output_samples.push_back(ptr_blocks[3][sample]); // BL
            m_output_samples.push_back(ptr_blocks[4][sample]); // BR
            break;
        case 7:
            m_output_samples.push_back(ptr_blocks[0][sample]); // FL
            m_output_samples.push_back(ptr_blocks[2][sample]); // FR
            m_output_samples.push_back(ptr_blocks[1][sample]); // FC
            m_output_samples.push_back(ptr_blocks[6][sample]); // LFE
            m_output_samples.push_back(ptr_blocks[5][sample]); // BC
            m_output_samples.push_back(ptr_blocks[3][sample]); // SL
            m_output_samples.push_back(ptr_blocks[4][sample]); // SR
            break;
        case 8:
            // 7.1 Vorbis to PCM (Decoding)
            // Vorbis             PCM
            // 0 Front Left   => 0 Front Left
            // 1 Front Center => 2 Front Right
            // 2 Front Right  => 1 Front Center
            // 3 Side Left    => 7 LFE
            // 4 Side Right   => 5 Back Left
            // 5 Back Left    => 6 Back Right
            // 6 Back Right   => 3 Side Left
            // 7 LFE          => 4 Side Right
            m_output_samples.push_back(ptr_blocks[0][sample]); // FL
            m_output_samples.push_back(ptr_blocks[2][sample]); // FR
            m_output_samples.push_back(ptr_blocks[1][sample]); // FC
            m_output_samples.push_back(ptr_blocks[7][sample]); // LFE
            m_output_samples.push_back(ptr_blocks[5][sample]); // BL
            m_output_samples.push_back(ptr_blocks[6][sample]); // BR
            m_output_samples.push_back(ptr_blocks[3][sample]); // SL
            m_output_samples.push_back(ptr_blocks[4][sample]); // SR
            break;
        case 1:
        case 2:
        case 4:
        default:
            // For mono/stereo/quadrophonic stereo/>8 channels: output in the
            // order libvorbis uses.  It's correct for the formats named, and
            // at present the Vorbis spec says streams w/>8 channels have user
            // defined channel order.
            for (int channel = 0; channel < vorbis_channels; ++channel)
                m_output_samples.push_back(ptr_blocks[channel][sample]);
    }
#endif