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

class AudioBuffer {
 public:
  enum {
    kConversionFailed = -3,
    kNoMemory = -2,
    kInvalidArg = -1,
    kSuccess = 0,
  };
  AudioBuffer();
  ~AudioBuffer();

  // Allocates storage for |ptr_data|, sets internal fields to values of
  // caller's args, and returns |kSuccess|. Returns |kInvalidArg| when
  // |ptr_data| is NULL.
  int Init(const AudioConfig& config,
           int64 timestamp,
           int64 duration,
           const uint8* ptr_data,
           int32 data_length);

  // Copies |AudioBuffer| data to |ptr_buffer|. Performs allocation if
  // necessary. Returns |kSuccess| when successful. Returns |kInvalidArg| when
  // |ptr_buffer| is NULL. Returns |kNoMemory| when memory allocation fails.
  int Clone(AudioBuffer* ptr_buffer) const;

  // Swaps |AudioBuffer| member data with |ptr_buffer|'s. The |AudioBuffer|s
  // must have non-NULL buffers.
  void Swap(AudioBuffer* ptr_buffer);

  // Accessors.
  int64 timestamp() const { return timestamp_; }
  int64 duration() const { return duration_; }
  uint8* buffer() const { return buffer_.get(); }
  int32 buffer_length() const { return buffer_length_; }
  int32 buffer_capacity() const { return buffer_capacity_; }
  const AudioConfig& config() const { return config_; }

 private:
  // Converts audio buffer from |kAudioFormatPcm| to |kAudioFormatIeeeFloat|,
  // and stores the output samples in |buffer_|. Returns |kSuccess| when
  // successful. Returns |kNoMemory| if unable to allocate storage for the
  // converted samples.
  int ConvertToIeeeFloat(const AudioConfig& config, const uint8* ptr_data);

  // Deinterleaves samples in |ptr_data| and returns |kSuccess|.
  int DeinterleaveSamples(const AudioConfig& config, const uint8* ptr_data);

  // Reorders multi channel audio samples to ensure that the samples are in
  // the order the libvorbis expects.
  int ReorderSamples(const AudioConfig& config, const uint8* ptr_data);

  int64 timestamp_;
  int64 duration_;
  boost::scoped_array<uint8> buffer_;
  int32 buffer_capacity_;
  int32 buffer_length_;
  AudioConfig config_;
  WEBMLIVE_DISALLOW_COPY_AND_ASSIGN(AudioBuffer);
};

// Pure interface class that provides a simple callback allowing the
// implementor class to receive |VideoFrame| pointers.
class AudioSamplesCallbackInterface {
 public:
  enum {
    // Returned by |OnSamplesReceived| when |ptr_frame| is NULL or empty.
    kInvalidArg = -2,
    kSuccess = 0,
  };
  virtual ~AudioSamplesCallbackInterface();

  // Passes an |AudioBuffer| pointer to the |AudioSamplesCallbackInterface|
  // implementation.
  virtual int32 OnSamplesReceived(const AudioBuffer* ptr_sample_buffer) = 0;
};

}  // namespace webmlive

#endif  // CLIENT_ENCODER_AUDIO_ENCODER_H_
