// Copyright (c) 2012 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef CLIENT_ENCODER_VORBIS_ENCODER_H_
#define CLIENT_ENCODER_VORBIS_ENCODER_H_

#include "client_encoder/audio_encoder.h"
#include "libvorbis/vorbis/codec.h"
#include "libvorbis/vorbis/vorbisenc.h"

namespace webmlive {

class VorbisEncoder {
 public:
  enum {
    kUnsupportedFormat = -2,
    kInvalidArg = -1,
    kSuccess = 0,
  };

  VorbisEncoder();
  ~VorbisEncoder();

  int Init(const AudioConfig& config);
  int EncodeBuffer(const AudioBuffer& uncompressed_buffer,
                   AudioBuffer* ptr_vorbis_buffer);
 private:
  vorbis_info info_;
  vorbis_dsp_state dsp_state_;
  vorbis_block block_;
  WEBMLIVE_DISALLOW_COPY_AND_ASSIGN(VorbisEncoder);
};

}  // namespace webmlive

#endif  // CLIENT_ENCODER_VORBIS_ENCODER_H_
