// Copyright (c) 2012 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef CLIENT_ENCODER_VORBIS_ENCODER_H_
#define CLIENT_ENCODER_VORBIS_ENCODER_H_

#include "boost/scoped_array.hpp"
#include "client_encoder/basictypes.h"
#include "client_encoder/audio_encoder.h"
#include "libvorbis/vorbis/codec.h"
#include "libvorbis/vorbis/vorbisenc.h"

namespace webmlive {

class VorbisEncoder {
 public:
  enum {
    // A libvorbis function returned an error.
    kCodecError = -202,

    // Internal error.
    kEncoderError = -201,

    // |audio_config| or |vorbis_config| format is not supported.
    kUnsupportedFormat = -200,
    kNoMemory = -2,
    kInvalidArg = -1,
    kSuccess = 0,

    // |ReadCompressedAudio()| has no samples available.
    kNoSamples = 1,
  };

  VorbisEncoder();
  ~VorbisEncoder();

  // Initializes libvorbis using the settings stored in |audio_config| and
  // |vorbis_config|. Returns |kSuccess| after successful libvorbis
  // initialization.
  int Init(const AudioConfig& audio_config, const VorbisConfig& vorbis_config);

  // Passes the samples in |uncompressed_buffer| to libvorbis. Returns
  // |kSuccess| after successful handoff of samples to the encoder.
  int Encode(const AudioBuffer& uncompressed_buffer);

  // Returns vorbis audio samples via |ptr_buffer| when libvorbis is able to
  // provide compressed data. Returns |kNoSamples| when libvorbis has no data
  // ready. Returns |kSuccess| when samples are written to |ptr_buffer|.
  int ReadCompressedAudio(AudioBuffer* ptr_buffer);

  // Accessors.
  const uint8* ident_header() const { return ident_header_.get(); }
  int32 ident_header_length() const { return ident_header_length_; }
  const uint8* comments_header() const { return comments_header_.get(); }
  int32 comments_header_length() const { return comments_header_length_; }
  const uint8* setup_header() const { return setup_header_.get(); }
  int32 setup_header_length() const { return setup_header_length_; }
  int64 audio_delay() const { return audio_delay_; }
  const AudioConfig* audio_config() const { return &audio_config_; }
  const VorbisConfig* vorbis_config() const { return &vorbis_config_; }

 private:
  // Reads the vorbis headers used to generate the WebM Vorbis track Codec
  // Private element. Stores header data in |ident_header_|,
  // |comments_header_|, and |setup_header_|. Returns |kSuccess| after
  // successful header generation.
  int GenerateHeaders();

  // Returns true when libvorbis has compressed samples available.
  bool SamplesAvailable();

  // Converts |num_samples| to milliseconds.
  int64 SamplesToMilliseconds(int64 num_samples) const;

  // Applies libvorbis encoder configuration values. Returns |kSuccess| when
  // libvorbis reports success.
  template <typename T> int CodecControl(int control_id, T val);

  vorbis_info info_;
  vorbis_dsp_state dsp_state_;
  vorbis_block block_;

  int32 ident_header_length_;
  int32 comments_header_length_;
  int32 setup_header_length_;
  int64 audio_delay_;
  int64 samples_encoded_;

  AudioConfig audio_config_;
  VorbisConfig vorbis_config_;

  boost::scoped_array<uint8> ident_header_;
  boost::scoped_array<uint8> comments_header_;
  boost::scoped_array<uint8> setup_header_;

  WEBMLIVE_DISALLOW_COPY_AND_ASSIGN(VorbisEncoder);
};

}  // namespace webmlive

#endif  // CLIENT_ENCODER_VORBIS_ENCODER_H_
