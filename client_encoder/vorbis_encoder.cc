// Copyright (c) 2012 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "client_encoder/vorbis_encoder.h"

#include <cstring>
#include <iterator>
#include <new>
#include <string>

#include "boost/shared_ptr.hpp"
#include "glog/logging.h"

namespace {

bool ValidOggPacket(const ogg_packet& packet) {
  return (packet.bytes > 0 && packet.packet);
}

// Stores payload data from |packet| in |ptr_storage|. Returns |kSuccess| after
// successful allocation of storage and copy of ogg_packet data.
int StorePacketPayload(const ogg_packet& packet,
                       boost::scoped_array<uint8>* ptr_storage,
                       int32* ptr_length) {
  using namespace webmlive;
  if (!ptr_storage || !ptr_length) {
    LOG(ERROR) << "cannot StoreHeaderPacket with NULL out param(s).";
    return VorbisEncoder::kInvalidArg;
  }
  if (!ValidOggPacket(packet)) {
    LOG(ERROR) << "cannot StoreHeaderPacket with invalid packet.";
    return VorbisEncoder::kInvalidArg;
  }
  ptr_storage->reset(new (std::nothrow) uint8[packet.bytes]);  // NOLINT
  if (!ptr_storage->get()) {
    LOG(ERROR) << "cannot StoreHeaderPacket, no memory.";
    return VorbisEncoder::kNoMemory;
  }
  //memcpy(ptr_storage->get(), packet.packet, packet.bytes);
  std::copy(packet.packet, packet.packet + packet.bytes, ptr_storage->get());
  *ptr_length = packet.bytes;
  return VorbisEncoder::kSuccess;
}

// Stores payload data from |packet| in |ptr_storage|. Returns |kSuccess| after
// successful allocation of storage and copy of ogg_packet data.
int StorePacketPayload(const ogg_packet& packet,
                       std::vector<uint8>* ptr_storage) {
  using namespace webmlive;
  if (!ptr_storage) {
    LOG(ERROR) << "cannot StoreDataPacket with NULL storage.";
    return VorbisEncoder::kInvalidArg;
  }
  if (!ValidOggPacket(packet)) {
    LOG(ERROR) << "cannot StoreDataPacket with invalid packet.";
    return VorbisEncoder::kInvalidArg;
  }
  std::copy(packet.packet, packet.packet + packet.bytes,
            std::back_inserter(*ptr_storage));
  return VorbisEncoder::kSuccess;
}

// Stores |packet| in |ptr_storage|. Returns |kSuccess| after successful
// allocation of storage and copy of ogg_packet data.
int StorePacket(const ogg_packet& packet, std::vector<uint8>* ptr_storage) {
  using namespace webmlive;
  if (!ptr_storage) {
    LOG(ERROR) << "cannot StoreDataPacket with NULL storage.";
    return VorbisEncoder::kInvalidArg;
  }
  if (!ValidOggPacket(packet)) {
    LOG(ERROR) << "cannot StoreDataPacket with invalid packet.";
    return VorbisEncoder::kInvalidArg;
  }
  const uint8* p = reinterpret_cast<const uint8*>(&packet);
  std::copy(p, p + sizeof(ogg_packet), std::back_inserter(*ptr_storage));
  return VorbisEncoder::kSuccess;
}

}  // namespace

namespace webmlive {

VorbisEncoder::VorbisEncoder()
    : ident_header_length_(0),
      comments_header_length_(0),
      setup_header_length_(0),
      audio_delay_(0),
      samples_encoded_(0),
      last_timestamp_(0),
      time_encoded_(0),
      first_input_timestamp_(-1),
      block_initialized_(false),
      dsp_initialized_(false),
      info_initialized_(false) {
  memset(&info_, 0, sizeof(info_));
  memset(&dsp_state_, 0, sizeof(dsp_state_));
  memset(&block_, 0, sizeof(block_));
}

VorbisEncoder::~VorbisEncoder() {
  if (dsp_initialized_) {
    vorbis_analysis_wrote(&dsp_state_, 0);
    vorbis_dsp_clear(&dsp_state_);
  }
  if (block_initialized_) {
    vorbis_block_clear(&block_);
  }
  if (info_initialized_) {
    vorbis_info_clear(&info_);
  }
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
  info_initialized_ = true;
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
  dsp_initialized_ = true;
  status = vorbis_block_init(&dsp_state_, &block_);
  if (status) {
    LOG(ERROR) << "vorbis_block_init failed: " << status;
    return kCodecError;
  }
  block_initialized_ = true;
  status = GenerateHeaders();
  if (status) {
    LOG(ERROR) << "GenerateHeaders failed: " << status;
    return kCodecError;
  }
  audio_config_ = audio_config;
  audio_config_.format_tag = kAudioFormatVorbis;
  vorbis_config_ = vorbis_config;
  return kSuccess;
}

int VorbisEncoder::Encode(const AudioBuffer& input_buffer) {
  if (!input_buffer.buffer()) {
    LOG(ERROR) << "cannot Encode empty input buffer!";
    return kInvalidArg;
  }
  if (first_input_timestamp_ == -1) {
    first_input_timestamp_ = input_buffer.timestamp();
    LOG(INFO) << "VorbisEncoder first_input_timestamp_="
              << first_input_timestamp_;
  }
  const AudioConfig& ac = input_buffer.config();
  const AudioBuffer& ib = input_buffer;
  const int num_blocks = ib.buffer_length() / ac.block_align;
  float** const ptr_encoder_buffer =
      vorbis_analysis_buffer(&dsp_state_, num_blocks);
  if (!ptr_encoder_buffer) {
    LOG(ERROR) << "cannot EncodeBuffer, no memory from libvorbis.";
    return kNoMemory;
  }

  // TODO(tomfinegan): Add a channel number to offset mapping similar to what
  //                   the ffmpeg libvorbis plugin uses to handle channel order
  //                   differences between uncompressed and vorbis audio.
  const int channels = ac.channels;
  if (ac.format_tag == kAudioFormatPcm) {
    // Deinterleave input samples, convert them to float, and store them in
    // |ptr_encoder_buffer|.
    const int16* const s16_pcm_samples = reinterpret_cast<int16*>(ib.buffer());
    for (int i = 0; i < num_blocks; ++i) {
      for (int c = 0; c < channels; ++c) {
        ptr_encoder_buffer[c][i] = s16_pcm_samples[i * channels + c] / 32768.f;
      }
    }
  } else {
    // Deinterleave input samples into |ptr_encoder_buffer|.
    const float* const ieee_float_samples =
        reinterpret_cast<float*>(ib.buffer());
    for (int i = 0; i < num_blocks; ++i) {
      for (int c = 0; c < channels; ++c) {
        ptr_encoder_buffer[c][i] = ieee_float_samples[i * channels + c];
      }
    }
  }
  vorbis_analysis_wrote(&dsp_state_, num_blocks);
  return kSuccess;
}

int VorbisEncoder::ReadCompressedAudio(AudioBuffer* ptr_buffer) {
  if (!ptr_buffer) {
    LOG(ERROR) << "ReadCompressedAudio requires a non-NULL ptr_buffer.";
    return kInvalidArg;
  }
  ogg_packet packet = {0};
  if (SamplesAvailable()) {
    // There's a compressed block available-- give libvorbis a chance to
    // optimize distribution of data for the current encode settings.
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
    while ((status = vorbis_bitrate_flushpacket(&dsp_state_, &packet)) == 1) {
      if (StorePacket(packet, &ogg_packets_)) {
        LOG(ERROR) << "StorePacket failed: " << status;
        return kCodecError;
      }
      if (StorePacketPayload(packet, &vorbis_samples_)) {
        LOG(ERROR) << "StorePacketPayload failed: " << status;
        return kCodecError;
      }
    }
  }
  if (ogg_packets_.size() < sizeof(packet) || vorbis_samples_.size() == 0) {
    return kNoSamples;
  }
  if (vorbis_samples_.size() < static_cast<size_t>(packet.bytes)) {
    LOG(ERROR) << "Error reading packets from libvorbis.";
    return kEncoderError;
  }

  // Use first packet with non-zero |granualpos| for delay.
  if (audio_delay_ == 0) {
    const int num_packets = ogg_packets_.size() / sizeof(packet);
    for (int i = 0; i < num_packets; ++i ) {
      const ogg_packet* const p =
          reinterpret_cast<ogg_packet*>(&ogg_packets_[i * sizeof(packet)]);
      if (packet.granulepos > 0) {
        audio_delay_ = SamplesToMilliseconds(p->granulepos);
        LOG(INFO) << "VorbisEncoder audio_delay_=" << audio_delay_;
        break;
      }
    }
  }

  // Use |granualpos| from the first packet returned by
  // |vorbis_bitrate_flushpacket()| to calculate |timestamp|.
  const ogg_packet* const p = reinterpret_cast<ogg_packet*>(&ogg_packets_[0]);
  const int64 timestamp =
      SamplesToMilliseconds(p->granulepos) + first_input_timestamp_;

  // |packet.granulepos| is the last complete sample in the packet, use it to
  // calculate |duration|.
  const int64 duration =
      SamplesToMilliseconds(packet.granulepos - samples_encoded_);
  const int status = ptr_buffer->Init(audio_config_,
                                      timestamp,
                                      duration,
                                      &vorbis_samples_[0],
                                      vorbis_samples_.size());
  if (status) {
    LOG(ERROR) << "AudioBuffer Init failed: " << status;
    return kCodecError;
  }
  LOG(INFO) << "ReadCompressedAudio\n"
      << "   samples_encoded_=" << samples_encoded_ << "\n"
      << "   timestamp(sec)=" << (timestamp / 1000.0) << "\n"
      << "   timestamp="      << timestamp << "\n"
      << "   duration(sec)= " << (duration / 1000.0) << "\n"
      << "   duration= "      << duration << "\n";
  last_timestamp_ = timestamp;
  samples_encoded_ = packet.granulepos;
  time_encoded_ = SamplesToMilliseconds(samples_encoded_);
  ogg_packets_.clear();
  vorbis_samples_.clear();
  return kSuccess;
}

// Clean up function used by |GenerateHeaders| to avoid having to repeatedly
// handle clean up of |vorbis_comment|s.
void ClearVorbisComments(vorbis_comment* ptr_comments) {
  if (ptr_comments) {
    vorbis_comment_clear(ptr_comments);
  }
}

int64 VorbisEncoder::time_encoded() const {
  if (first_input_timestamp_ < 0) {
    return 0;
  }
  return first_input_timestamp_ + time_encoded_;
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
  status = StorePacketPayload(ident_packet,
                              &ident_header_,
                              &ident_header_length_);
  if (status) {
    LOG(ERROR) << "cannot store ident header: " << status;
    return status;
  }
  status = StorePacketPayload(comments_packet,
                              &comments_header_,
                              &comments_header_length_);
  if (status) {
    LOG(ERROR) << "cannot store comments header: " << status;
    return status;
  }
  status = StorePacketPayload(setup_packet,
                              &setup_header_,
                              &setup_header_length_);
  if (status) {
    LOG(ERROR) << "cannot store setup header: " << status;
    return status;
  }
  return kSuccess;
}

// When |SamplesAvailable()| returns true, the user must consume all samples
// made available by libvorbis. Any compressed samples left unconsumed will be
// lost.
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
  if (sample_rate != 0) {
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
