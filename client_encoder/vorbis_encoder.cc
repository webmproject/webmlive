// Copyright (c) 2012 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "client_encoder/vorbis_encoder.h"

#include <cstring>

#include "glog/logging.h"
#include "client_encoder/webm_encoder.h"

namespace webmlive {

VorbisEncoder::VorbisEncoder() {
  memset(&info_, 0, sizeof(info_));
  memset(&dsp_state_, 0, sizeof(dsp_state_));
  memset(&block_, 0, sizeof(block_));
}

VorbisEncoder::~VorbisEncoder() {
  vorbis_analysis_wrote(&dsp_state_, 0);
  vorbis_block_clear(&block_);
  vorbis_dsp_clear(&dsp_state_);
  vorbis_info_clear(&info_);
}

// Bitrate values are multiplied by 1000. |WebmEncoderConfig| and its children
// express bitrates in kilobits. Libvorbis bitrates are in bits.
int VorbisEncoder::Init(const WebmEncoderConfig& config) {
  if (config.actual_audio_config.channels > 2) {
    LOG(ERROR) << "audio input with more than 2 channels is not supported.";
    return kUnsupportedFormat;
  }
  const uint16& format_tag = config.actual_audio_config.format_tag;
  if (format_tag != kAudioFormatPcm && format_tag != kAudioFormatIeeeFloat) {
    LOG(ERROR) << "input must be uncompressed.";
    return kUnsupportedFormat;
  }
  if (format_tag != kAudioFormatIeeeFloat &&
      config.actual_audio_config.bits_per_sample != 16) {
    LOG(ERROR) << "PCM input must be 16 bits per sample.";
    return kUnsupportedFormat;
  }
  vorbis_info_init(&info_);
  const VorbisConfig& vc = config.vorbis_config;
  int minimum_bitrate = -1;
  int maximum_bitrate = -1;
  if (vc.minimum_bitrate != VorbisConfig::kUseDefault &&
      vc.maximum_bitrate != VorbisConfig::kUseDefault) {
    minimum_bitrate = vc.minimum_bitrate * 1000;
    maximum_bitrate = vc.maximum_bitrate * 1000;
  }
  const AudioConfig& ac = config.actual_audio_config;
  int status = vorbis_encode_setup_managed(&info_,
                                           ac.channels,
                                           ac.sample_rate,
                                           minimum_bitrate,
                                           vc.average_bitrate * 1000,
                                           maximum_bitrate);
  if (status) {
    LOG(ERROR) << "vorbis_encode_setup_managed failed: " << status;
    return kCodecError;
  }
  if (minimum_bitrate == -1 && maximum_bitrate == -1) {
    // Disable variable rate control.
    if (CodecControl(OV_ECTL_RATEMANAGE2_SET, NULL)) {
      return kCodecError;
    }
  }
  if (vc.channel_coupling) {
    const int enable_coupling = 1;
    if (CodecControl(OV_ECTL_COUPLING_SET, enable_coupling)) {
      return kCodecError;
    }
  }
  if (CodecControl(OV_ECTL_IBLOCK_SET, vc.impulse_block_bias)) {
    return kCodecError;
  }
  if (CodecControl(OV_ECTL_LOWPASS_SET, vc.lowpass_frequency)) {
    return kCodecError;
  }
  status = vorbis_encode_setup_init(&info_);
  if (status) {
    LOG(ERROR) << "vorbis_encode_setup_init failed: " << status;
    return kCodecError;
  }
  status = vorbis_analysis_init(&dsp_state_, &info_);
  if (status) {
    LOG(ERROR) << "vorbis_analysis_init failed: " << status;
    return kCodecError;
  }
  status = vorbis_block_init(&dsp_state_, &block_);
  if (status) {
    LOG(ERROR) << "vorbis_block_init failed: " << status;
    return kCodecError;
  }
  return kSuccess;
}

int VorbisEncoder::EncodeBuffer(const AudioBuffer& input_buffer,
                                AudioBuffer* ptr_vorbis_buffer) {
  if (!ptr_vorbis_buffer) {
    LOG(ERROR) << "EncoderBuffer requires non-null vorbis buffer.";
    return kInvalidArg;
  }
  const int num_samples =
      input_buffer.buffer_length() / input_buffer.config().bits_per_sample;
  float** const ptr_encoder_buffer =
      vorbis_analysis_buffer(&dsp_state_, num_samples);
  if (ptr_encoder_buffer) {
    if (input_buffer.config().format_tag == kAudioFormatPcm) {
      // deinterleave pcm_buffer/convert sample to float/store in encoder buf
      const int16* const s16_pcm_samples =
          reinterpret_cast<int16*>(input_buffer.buffer());
      for (int i = 0; i < num_samples; ++i) {
        ptr_encoder_buffer[0][i] = s16_pcm_samples[i] / 32768.f;
        ptr_encoder_buffer[1][i] = s16_pcm_samples[i+1] / 32768.f;
      }
    } else {
      const float* const ieee_float_samples =
          reinterpret_cast<float*>(input_buffer.buffer());
      // deinterleave into encoder buf
      for (int i = 0; i < num_samples; ++i) {
        ptr_encoder_buffer[0][i] = ieee_float_samples[i];
        ptr_encoder_buffer[1][i] = ieee_float_samples[i+1];
      }
    }
    vorbis_analysis_wrote(&dsp_state_, num_samples);
  }
  return kSuccess;
}

template <typename T>
int VorbisEncoder::CodecControl(int control_id, T val) {
  int status = kSuccess;
  if (control_id == OV_ECTL_RATEMANAGE2_SET) {
    status = vorbis_encode_ctl(&info_, control_id, NULL);
  } else {
    status = vorbis_encode_ctl(&info_, control_id, &val);
  }
  if (status) {
    LOG(ERROR) << "vorbis_encode_ctl (" << control_id << ") failed: "
               << status;
    status = kCodecError;
  }
  return status;
}

}  // namespace webmlive
