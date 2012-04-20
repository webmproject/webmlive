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
#include "client_encoder/webm_encoder.h"
#include "libvorbis/vorbis/codec.h"
#include "libvorbis/vorbis/vorbisenc.h"

namespace webmlive {

class VorbisEncoder {
 public:
  enum {
    kCodecError = -202,
    kEncoderError = -201,
    kUnsupportedFormat = -200,
    kInvalidArg = -1,
    kSuccess = 0,
    kNeedMoreSamples = 1,
  };

  VorbisEncoder();
  ~VorbisEncoder();

  int Init(const WebmEncoderConfig& config);
  int EncodeBuffer(const AudioBuffer& uncompressed_buffer,
                   AudioBuffer* ptr_vorbis_buffer);
 private:
  template <typename T> int CodecControl(int control_id, T val);
  vorbis_info info_;
  vorbis_dsp_state dsp_state_;
  vorbis_block block_;
  WEBMLIVE_DISALLOW_COPY_AND_ASSIGN(VorbisEncoder);
};

}  // namespace webmlive

#endif  // CLIENT_ENCODER_VORBIS_ENCODER_H_
