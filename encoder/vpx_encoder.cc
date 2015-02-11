// Copyright (c) 2012 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#if defined _MSC_VER
// Disable warning C4505(unreferenced local function has been removed) in MSVC.
// At the time this comment was written the warning is emitted 27 times for
// vp8.h and vp8cx.h (included by vpx_encoder.h).
#pragma warning(disable:4505)
#endif
#include "encoder/vpx_encoder.h"

#include "encoder/webm_encoder.h"
#include "glog/logging.h"

namespace webmlive {

VpxEncoder::VpxEncoder()
    : frames_in_(0),
      frames_out_(0),
      last_keyframe_time_(0),
      last_timestamp_(0) {
  memset(&vp8_context_, 0, sizeof(vp8_context_));
}

VpxEncoder::~VpxEncoder() {
  vpx_codec_destroy(&vp8_context_);
}

// Populates libvpx configuration structure with user values, and initializes
// the library for VP8 encoding.
int VpxEncoder::Init(const WebmEncoderConfig& user_config) {
  vpx_codec_enc_cfg_t libvpx_config = {0};
  vpx_codec_err_t status = vpx_codec_enc_config_default(vpx_codec_vp8_cx(),
                                                        &libvpx_config, 0);
  if (status) {
    LOG(ERROR) << "vpx_codec_enc_config_default failed: "
               << vpx_codec_err_to_string(status);
    return VideoEncoder::kCodecError;
  }
  config_ = user_config.vpx_config;
  libvpx_config.g_pass = VPX_RC_ONE_PASS;
  libvpx_config.g_timebase.num = 1;
  libvpx_config.g_timebase.den = kTimebase;
  libvpx_config.rc_end_usage = VPX_CBR;

  // TODO(tomfinegan): Add user settings validation-- v1 was relying on the
  //                   DShow filter to check settings.

  // Copy user configuration values into libvpx configuration struct
  libvpx_config.g_h = user_config.actual_video_config.height;
  libvpx_config.g_w = user_config.actual_video_config.width;
  libvpx_config.rc_target_bitrate = config_.bitrate;
  libvpx_config.rc_min_quantizer = config_.min_quantizer;
  libvpx_config.rc_max_quantizer = config_.max_quantizer;

  // Leave the libvpx thread count and undershoot settings alone if not
  // specified by the user.
  if (config_.thread_count != VpxConfig::kUseDefault) {
    libvpx_config.g_threads = config_.thread_count;
  }
  if (config_.undershoot != VpxConfig::kUseDefault) {
    libvpx_config.rc_undershoot_pct = config_.undershoot;
  }

  // Configure the codec library.
  status =
      vpx_codec_enc_init(&vp8_context_, vpx_codec_vp8_cx(), &libvpx_config, 0);
  if (status) {
    LOG(ERROR) << "vpx_codec_enc_init failed: "
               << vpx_codec_err_to_string(status);
    return VideoEncoder::kCodecError;
  }

  // Pass the remaining configuration settings into libvpx, but leave them at
  // the library defaults if not specified by the user.
  if (CodecControl(VP8E_SET_CPUUSED, config_.speed, VpxConfig::kUseDefault)) {
      return VideoEncoder::kCodecError;
  }
  if (CodecControl(VP8E_SET_STATIC_THRESHOLD, config_.static_threshold,
                   VpxConfig::kUseDefault)) {
      return VideoEncoder::kCodecError;
  }
  if (CodecControl(VP8E_SET_TOKEN_PARTITIONS, config_.token_partitions,
                   VpxConfig::kUseDefault)) {
      return VideoEncoder::kCodecError;
  }
  if (CodecControl(VP8E_SET_NOISE_SENSITIVITY, config_.noise_sensitivity,
                   VpxConfig::kUseDefault)) {
      return VideoEncoder::kCodecError;
  }
  return kSuccess;
}

// Encodes |ptr_raw_frame| using libvpx and stores the resulting VP8 frame in
// |ptr_vp8_frame|. First checks if |ptr_raw_frame| should be dropped due to
// decimation, and then checks if it's time to force a keyframe before finally
// wrapping the data from |ptr_raw_frame| in a vpx_img_t struct and passing it
// to libvpx.
int VpxEncoder::EncodeFrame(const VideoFrame& raw_frame,
                            VideoFrame* ptr_vp8_frame) {
  if (!raw_frame.buffer()) {
    LOG(ERROR) << "NULL raw VideoFrame buffer!";
    return kInvalidArg;
  }
  if (raw_frame.format() != kVideoFormatI420 &&
      raw_frame.format() != kVideoFormatYV12) {
    LOG(ERROR) << "Unsupported VideoFrame format!";
    return kInvalidArg;
  }
  ++frames_in_;

  // If decimation is enabled, determine if it's time to drop a frame.
  if (config_.decimate > 1) {
    const int drop_frame = frames_in_ % config_.decimate;

    // Non-zero |drop_frame| values mean drop the frame.
    if (drop_frame) {
      return kDropped;
    }
  }

  // Determine if it's time to force a keyframe.
  const int64 time_since_keyframe =
      raw_frame.timestamp() - last_keyframe_time_;
  const bool force_keyframe = time_since_keyframe > config_.keyframe_interval;

  // Use the |vpx_img_wrap| to wrap the buffer within |ptr_raw_frame| in
  // |vpx_image| for passing the buffer to libvpx.
  const VideoFormat video_format = raw_frame.format();
  const vpx_img_fmt vpx_image_format =
      (video_format == kVideoFormatI420) ? VPX_IMG_FMT_I420 : VPX_IMG_FMT_YV12;
  vpx_image_t vpx_image;
  vpx_image_t* const ptr_vpx_image = vpx_img_wrap(&vpx_image,
                                                  vpx_image_format,
                                                  raw_frame.width(),
                                                  raw_frame.height(),
                                                  1,  // Alignment.
                                                  raw_frame.buffer());

  const vpx_enc_frame_flags_t flags = force_keyframe ? VPX_EFLAG_FORCE_KF : 0;
  const uint32 duration = static_cast<uint32>(raw_frame.duration());

  // Pass |ptr_raw_frame|'s data to libvpx.
  const vpx_codec_err_t vpx_status =
      vpx_codec_encode(&vp8_context_, ptr_vpx_image, raw_frame.timestamp(),
                       duration, flags, VPX_DL_REALTIME);
  if (vpx_status) {
    LOG(ERROR) << "EncodeFrame vpx_codec_encode failed: "
               << vpx_codec_err_to_string(vpx_status);
    return kCodecError;
  }

  // Consume output packets from libvpx. Note that the library may emit stats
  // packets in addition to the compressed data.
  vpx_codec_iter_t iter = NULL;
  for (;;) {
    const vpx_codec_cx_pkt_t* pkt =
        vpx_codec_get_cx_data(&vp8_context_, &iter);
    if (!pkt) {
      break;
    }
    const bool compressed_frame_packet = pkt->kind == VPX_CODEC_CX_FRAME_PKT;

    // Copy the compressed data to |ptr_vp8_frame|.
    if (compressed_frame_packet) {
      const bool is_keyframe = !!(pkt->data.frame.flags & VPX_FRAME_IS_KEY);
      uint8* const ptr_vp8_frame_buf =
          reinterpret_cast<uint8*>(pkt->data.frame.buf);
      VideoConfig vp8_config = raw_frame.config();
      vp8_config.format = kVideoFormatVP8;
      const int32 status = ptr_vp8_frame->Init(vp8_config,
                                               is_keyframe,
                                               raw_frame.timestamp(),
                                               raw_frame.duration(),
                                               ptr_vp8_frame_buf,
                                               pkt->data.frame.sz);
      if (status) {
        LOG(ERROR) << "VideoFrame Init failed: " << status;
        return kEncoderError;
      }
      if (is_keyframe) {
        last_keyframe_time_ = ptr_vp8_frame->timestamp();
        LOG(INFO) << "keyframe @ " << last_keyframe_time_ / 1000.0 << "sec ("
                  << last_keyframe_time_ << "ms)";
      }
      ++frames_out_;
      break;
    }
  }
  last_timestamp_ = ptr_vp8_frame->timestamp();
  return kSuccess;
}

template <typename T>
int VpxEncoder::CodecControl(int control_id, T val, T default_val) {
  if (val != default_val) {
    vpx_codec_err_t status = VPX_CODEC_OK;

    // libvpx's |vpx_codec_control| function features a bit of preprocessor
    // abuse, and the function call will not expand properly in the context
    // of a template function without use of the |vp8e_enc_control_id| enum
    // entries. Verbosity plus switch to the rescue!
    switch (control_id) {
      case VP8E_SET_CPUUSED:
        status = vpx_codec_control(&vp8_context_, VP8E_SET_CPUUSED, val);
        break;
      case VP8E_SET_STATIC_THRESHOLD:
        status = vpx_codec_control(&vp8_context_, VP8E_SET_STATIC_THRESHOLD,
                                   val);
        break;
      case VP8E_SET_TOKEN_PARTITIONS:
        status = vpx_codec_control(&vp8_context_, VP8E_SET_TOKEN_PARTITIONS,
                                   static_cast<vp8e_token_partitions>(val));
        break;
      case VP8E_SET_NOISE_SENSITIVITY:
        status = vpx_codec_control(&vp8_context_, VP8E_SET_NOISE_SENSITIVITY,
                                   val);
      default:
        LOG(ERROR) << "unknown control id in VpxEncoder::CodecControl.";
        return kEncoderError;
    }

    if (status) {
      LOG(ERROR) << "vpx_codec_control (" << control_id << ") failed: "
                 << vpx_codec_err_to_string(status);
      return VideoEncoder::kCodecError;
    }
  }
  return kSuccess;
}

}  // namespace webmlive
