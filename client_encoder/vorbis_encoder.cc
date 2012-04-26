// Copyright (c) 2012 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "client_encoder/vorbis_encoder.h"

#include <cstring>
#include <new>
#include <string>

#include "boost/shared_ptr.hpp"
#include "glog/logging.h"

namespace webmlive {

// Stores |packet|'s data in |ptr_storage|. Returns |kSuccess| after successful
// allocation of storage and copy of ogg_packet data.
int StorePacket(const ogg_packet& packet,
                boost::scoped_array<uint8>* ptr_storage,
                int32* ptr_length) {
  if (!ptr_storage || !ptr_length) {
    LOG(ERROR) << "cannot StorePacket with NULL out param(s).";
    return VorbisEncoder::kInvalidArg;
  }
  ptr_storage->reset(new (std::nothrow) uint8[packet.bytes]);  // NOLINT
  if (!ptr_storage->get()) {
    LOG(ERROR) << "cannot StorePacket, no memory.";
    return VorbisEncoder::kNoMemory;
  }
  memcpy(ptr_storage->get(), packet.packet, packet.bytes);
  *ptr_length = packet.bytes;
  return VorbisEncoder::kSuccess;
}

VorbisEncoder::VorbisEncoder()
    : ident_header_length_(0),
      comments_header_length_(0),
      setup_header_length_(0),
      audio_delay_(-1),
      samples_encoded_(0) {
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
int VorbisEncoder::Init(const AudioConfig& audio_config,
                        const VorbisConfig& vorbis_config) {
  if (audio_config.channels <= 0 || audio_config.channels > 2) {
    LOG(ERROR) << "invalid/unsupported number of audio channels.";
    return kUnsupportedFormat;
  }
  const uint16& format_tag = audio_config.format_tag;
  if (format_tag != kAudioFormatPcm && format_tag != kAudioFormatIeeeFloat) {
    LOG(ERROR) << "input must be uncompressed.";
    return kUnsupportedFormat;
  }
  if (format_tag == kAudioFormatPcm && audio_config.bits_per_sample != 16) {
    LOG(ERROR) << "PCM input must be 16 bits per sample.";
    return kUnsupportedFormat;
  }
  const int kBitsPerIeeeFloat = sizeof(float) * 8;  // NOLINT(runtime/sizeof)
  if (format_tag == kAudioFormatIeeeFloat &&
      audio_config.bits_per_sample != kBitsPerIeeeFloat) {
    LOG(ERROR) << "IEEE floating point input must be 32 bits per sample.";
    return kUnsupportedFormat;
  }
  vorbis_info_init(&info_);
  const VorbisConfig& vc = vorbis_config;
  int minimum_bitrate = -1;
  int maximum_bitrate = -1;
  if (vc.minimum_bitrate != VorbisConfig::kUseDefault &&
      vc.maximum_bitrate != VorbisConfig::kUseDefault) {
    minimum_bitrate = vc.minimum_bitrate * 1000;
    maximum_bitrate = vc.maximum_bitrate * 1000;
  }
  int status = vorbis_encode_setup_managed(&info_,
                                           audio_config.channels,
                                           audio_config.sample_rate,
                                           minimum_bitrate,
                                           vc.average_bitrate * 1000,
                                           maximum_bitrate);
  if (status) {
    LOG(ERROR) << "vorbis_encode_setup_managed failed: " << status;
    return kCodecError;
  }
  if (minimum_bitrate == -1 && maximum_bitrate == -1 &&
      vc.bitrate_based_quality) {
    // Enable VBR.
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
  audio_config_ = audio_config;
  audio_config_.format_tag = kAudioFormatVorbis;
  vorbis_config_ = vorbis_config;
  return GenerateHeaders();
}

int VorbisEncoder::Encode(const AudioBuffer& input_buffer) {
  const AudioBuffer& ib = input_buffer;
  const int bytes_per_sample = (ib.config().bits_per_sample + 7) / 8;
  const int num_samples =
      (ib.buffer_length() + (bytes_per_sample - 1)) / bytes_per_sample;
  float** const ptr_encoder_buffer =
      vorbis_analysis_buffer(&dsp_state_, num_samples);
  if (!ptr_encoder_buffer) {
    LOG(ERROR) << "cannot EncodeBuffer, no memory from libvorbis.";
    return kNoMemory;
  }
  if (ib.config().format_tag == kAudioFormatPcm) {
    const int16* const s16_pcm_samples = reinterpret_cast<int16*>(ib.buffer());
    if (ib.config().channels == 2) {
      for (int i = 0; i < num_samples; i += 2) {
        // Deinterleave input samples, convert them to float, and store them in
        // |ptr_encoder_buffer|.
        ptr_encoder_buffer[0][i] = s16_pcm_samples[i] / 32768.f;
        ptr_encoder_buffer[1][i] = s16_pcm_samples[i+1] / 32768.f;
      }
    } else {
      for (int i = 0; i < num_samples; ++i) {
        // Convert input samples to float and store.
        ptr_encoder_buffer[0][i] = s16_pcm_samples[i] / 32768.f;
      }
    }
  } else {
    const float* const ieee_float_samples =
        reinterpret_cast<float*>(ib.buffer());
    if (ib.config().channels == 2) {
      // Deinterleave input samples into |ptr_encoder_buffer|.
      for (int i = 0; i < num_samples; i += 2) {
        ptr_encoder_buffer[0][i] = ieee_float_samples[i];
        ptr_encoder_buffer[1][i] = ieee_float_samples[i+1];
      }
    } else {
      memcpy(ptr_encoder_buffer, ieee_float_samples, ib.buffer_length());
    }
  }
  vorbis_analysis_wrote(&dsp_state_, num_samples);
  return kSuccess;
}

int VorbisEncoder::ReadCompressedAudio(AudioBuffer* ptr_buffer) {
  if (!ptr_buffer) {
    LOG(ERROR) << "ReadCompressedAudio requires a non-NULL ptr_buffer.";
    return kInvalidArg;
  }
  if (!SamplesAvailable()) {
    return kNoSamples;
  }

  // There's a compressed block available-- give libvorbis a chance to
  // optimize distribution of data for the current encode settings.
  ogg_packet packet = {0};
  int status = vorbis_analysis(&block_, &packet);
  if (status) {
    LOG(ERROR) << "vorbis_analysis failed: " << status;
    return kCodecError;
  }
  status = vorbis_bitrate_addblock(&block_);
  if (status) {
    LOG(ERROR) << "vorbis_bitrate_addblock failed: " << status;
    return kCodecError;
  }

  // Read the packet and copy it to the user's buffer.
  status = vorbis_bitrate_flushpacket(&dsp_state_, &packet);
  if (status) {
    LOG(ERROR) << "vorbis_bitrate_flushpacket failed: " << status;
    return kCodecError;
  }
  if (audio_delay_ == -1) {
    audio_delay_ = SamplesToMilliseconds(packet.granulepos);
  }
  const int64 timestamp = SamplesToMilliseconds(samples_encoded_);
  const int64 duration =
      SamplesToMilliseconds(packet.granulepos - samples_encoded_);
  samples_encoded_ = packet.granulepos;
  status = ptr_buffer->Init(audio_config_,
                            timestamp,
                            duration,
                            packet.packet,
                            packet.bytes);
  if (status) {
    LOG(ERROR) << "AudioBuffer Init failed: " << status;
    return kCodecError;
  }
  return status;
}

// Clean up function used by |GenerateHeaders| to avoid having to repeatedly
// handle clean up of |vorbis_comment|s.
void ClearVorbisComments(vorbis_comment* ptr_comments) {
  if (ptr_comments) {
    vorbis_comment_clear(ptr_comments);
  }
}

int VorbisEncoder::GenerateHeaders() {
  vorbis_comment comments = {0};
  vorbis_comment_init(&comments);
  // Abuse |boost::shared_ptr| to avoid repeating the call to
  // |vorbis_comment_clear| for every failure in this method.
  boost::shared_ptr<vorbis_comment> comments_auto_clear(&comments,
                                                        ClearVorbisComments);

  // Add app name and version to vorbis comments.
  std::string encoder_id = kClientName;
  encoder_id += " v";
  encoder_id += kClientVersion;
  const std::string kVorbisEncoderTag = "encoder";
  vorbis_comment_add_tag(&comments,
                         kVorbisEncoderTag.c_str(),
                         encoder_id.c_str());

  // Generate the vorbis header packets.
  ogg_packet ident_packet = {0}, comments_packet = {0}, setup_packet = {0};
  int status = vorbis_analysis_headerout(&dsp_state_,
                                         &comments,
                                         &ident_packet,
                                         &comments_packet,
                                         &setup_packet);
  if (status) {
    LOG(ERROR) << "vorbis_analysis_headerout failed: " << status;
    return kCodecError;
  }

  // Store the header packet data.
  status = StorePacket(ident_packet, &ident_header_, &ident_header_length_);
  if (status) {
    LOG(ERROR) << "cannot store ident header: " << status;
    return status;
  }
  status = StorePacket(comments_packet,
                       &comments_header_,
                       &comments_header_length_);
  if (status) {
    LOG(ERROR) << "cannot store comments header: " << status;
    return status;
  }
  status = StorePacket(setup_packet, &setup_header_, &setup_header_length_);
  if (status) {
    LOG(ERROR) << "cannot store setup header: " << status;
    return status;
  }
  return kSuccess;
}

bool VorbisEncoder::SamplesAvailable() {
  const int kSamplesAvailable = 1;
  const int status = vorbis_analysis_blockout(&dsp_state_, &block_);
  if (status == kSamplesAvailable) {
    return true;
  }
  return false;
}

int64 VorbisEncoder::SamplesToMilliseconds(int64 num_samples) const {
  const double sample_rate = audio_config_.sample_rate;
  const double sample_count = static_cast<double>(num_samples);
  double seconds = 0;
  if (sample_count != 0) {
    seconds = sample_count / sample_rate;
  }
  return static_cast<int64>(seconds * 1000);
}

template <typename T>
int VorbisEncoder::CodecControl(int control_id, T val) {
  int status = kSuccess;
  if (control_id == OV_ECTL_RATEMANAGE2_SET && val == 0) {
    // Special case disabling rate control-- libvorbis expects a NULL pointer.
    status = vorbis_encode_ctl(&info_, control_id, NULL);
  } else if (val != VorbisConfig::kUseDefault) {
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
