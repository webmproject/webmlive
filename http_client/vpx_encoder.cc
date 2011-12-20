// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
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
#include "http_client/vpx_encoder.h"

#include "glog/logging.h"
#include "http_client/webm_encoder.h"

namespace webmlive {

VpxEncoder::VpxEncoder()
    : frames_in_(0), frames_out_(0), last_keyframe_time_(0) {
  memset(&vp8_context_, 0, sizeof(vp8_context_));
}

VpxEncoder::~VpxEncoder() {
  vpx_codec_destroy(&vp8_context_);
}

// Populates libvpx configuration structure with user values, and initializes
// the library for VP8 encoding.
int VpxEncoder::Init(const WebmEncoderConfig* ptr_user_config) {
  if (!ptr_user_config) {
    LOG(ERROR) << "VpxEncoder cannot Init with NULL VpxConfig!";
    return kInvalidArg;
  }
  vpx_codec_enc_cfg_t libvpx_config = {0};
  vpx_codec_err_t status = vpx_codec_enc_config_default(vpx_codec_vp8_cx(),
                                                        &libvpx_config, 0);
  if (status) {
    LOG(ERROR) << "vpx_codec_enc_config_default failed: "
               << vpx_codec_err_to_string(status);
    return VideoEncoder::kCodecError;
  }
  config_ = ptr_user_config->vpx_config;

  // TODO(tomfinegan): Add settings validation-- v1 was relying on the DShow
  //                   filter to check settings.

  // Copy user configuration values into libvpx configuration struct.
  libvpx_config.g_h = ptr_user_config->video_config.height;
  libvpx_config.g_w = ptr_user_config->video_config.width;
  libvpx_config.g_pass = VPX_RC_ONE_PASS;
  libvpx_config.g_timebase.num = 1;
  libvpx_config.g_timebase.den = kTimebase;
  libvpx_config.rc_end_usage = VPX_CBR;
  libvpx_config.rc_target_bitrate = config_.bitrate;
  libvpx_config.rc_min_quantizer = config_.min_quantizer;
  libvpx_config.rc_max_quantizer = config_.max_quantizer;

  // Leave the libvpx thread count and undershoot settings alone if not
  // specified by the user.
  if (config_.thread_count != kDefaultVpxThreadCount) {
    libvpx_config.g_threads = config_.thread_count;
  }
  if (config_.undershoot != kDefaultVpxUndershoot) {
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
  if (config_.speed != kDefaultVpxSpeed) {
    status = vpx_codec_control(&vp8_context_, VP8E_SET_CPUUSED, config_.speed);
    if (status) {
      LOG(ERROR) << "vpx_codec_control VP8E_SET_CPUUSED (speed) failed: "
                 << vpx_codec_err_to_string(status);
      return VideoEncoder::kCodecError;
    }
  }
  if (config_.static_threshold != kDefaultVpxStaticThreshold) {
    status = vpx_codec_control(&vp8_context_, VP8E_SET_STATIC_THRESHOLD,
                               config_.static_threshold);
    if (status) {
      LOG(ERROR) << "vpx_codec_control VP8E_SET_STATIC_THRESHOLD failed: "
                 << vpx_codec_err_to_string(status);
      return VideoEncoder::kCodecError;
    }
  }
  if (config_.token_partitions != kDefaultVpxTokenPartitions) {
    const vp8e_token_partitions token_partitions =
        static_cast<vp8e_token_partitions>(config_.token_partitions);
    status = vpx_codec_control(&vp8_context_, VP8E_SET_TOKEN_PARTITIONS,
                               token_partitions);
    if (status) {
      LOG(ERROR) << "vpx_codec_control VP8E_SET_TOKEN_PARTITIONS failed: "
                 << vpx_codec_err_to_string(status);
      return VideoEncoder::kCodecError;
    }
  }
  return kSuccess;
}

// Encodes |ptr_raw_frame| using libvpx and stores the resulting VP8 frame in
// |ptr_vp8_frame|. First checks if |ptr_raw_frame| should be dropped due to
// decimation, and then checks if it's time to force a keyframe before finally
// wrapping the data from |ptr_raw_frame| in a vpx_img_t struct and passing it
// to libvpx.
int32 VpxEncoder::EncodeFrame(const VideoFrame* const ptr_raw_frame,
                              VideoFrame* ptr_vp8_frame) {
  if (!ptr_raw_frame || !ptr_raw_frame->buffer()) {
    LOG(ERROR) << "NULL VideoFrame or VideoFrame with NULL buffer!";
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
      ptr_raw_frame->timestamp() - last_keyframe_time_;
  const bool force_keyframe =
      (time_since_keyframe > config_.keyframe_interval) ? true : false;

  // Use the |vpx_img_wrap| to wrap the buffer within |ptr_raw_frame| in
  // |vpx_image| for passing the buffer to libvpx.
  vpx_image_t vpx_image;
  vpx_image_t* const ptr_vpx_image = vpx_img_wrap(&vpx_image, VPX_IMG_FMT_I420,
                                                  ptr_raw_frame->width(),
                                                  ptr_raw_frame->height(),
                                                  1,  // Alignment.
                                                  ptr_raw_frame->buffer());
  DCHECK_EQ(&vpx_image, ptr_vpx_image);

  const vpx_enc_frame_flags_t flags = force_keyframe ? VPX_EFLAG_FORCE_KF : 0;
  const uint32 duration = static_cast<uint32>(ptr_raw_frame->duration());

  // Pass |ptr_raw_frame|'s data to libvpx.
  const vpx_codec_err_t vpx_status =
      vpx_codec_encode(&vp8_context_, ptr_vpx_image,
                       ptr_raw_frame->timestamp(), duration, flags,
                       VPX_DL_REALTIME);
  if (vpx_status) {
    LOG(ERROR) << "EncodeFrame vpx_codec_encode failed: "
               << vpx_codec_err_to_string(vpx_status);
    return kCodecError;
  }

  // Consume output packets from libvpx. Note that the library may emit stats
  // packets in addition to the compressed data.
  vpx_codec_iter_t iter = NULL;
  for (;;) {
    const vpx_codec_cx_pkt_t* pkt = vpx_codec_get_cx_data(&vp8_context_,
                                                          &iter);
    if (!pkt) {
      break;
    }
    bool compressed_frame_packet = false;
    switch (pkt->kind) {
    case VPX_CODEC_CX_FRAME_PKT:
      compressed_frame_packet = true;
      break;
    default:
      break;
    }

    // Copy the compressed data to |ptr_vp8_frame|.
    if (compressed_frame_packet) {
      const bool is_keyframe = pkt->data.frame.flags & VPX_FRAME_IS_KEY;
      uint8* ptr_vp8_frame_buf = reinterpret_cast<uint8*>(pkt->data.frame.buf);
      const int32 status = ptr_vp8_frame->Init(kVideoFormatVP8, is_keyframe,
                                               ptr_raw_frame->width(),
                                               ptr_raw_frame->height(),
                                               ptr_raw_frame->timestamp(),
                                               ptr_raw_frame->duration(),
                                               ptr_vp8_frame_buf,
                                               pkt->data.frame.sz);
      if (status) {
        LOG(ERROR) << "VideoFrame Init failed: " << status;
        return kEncoderError;
      }
      if (is_keyframe) {
        last_keyframe_time_ = ptr_vp8_frame->timestamp();
      }
      ++frames_out_;
      break;
    }
  }
  return kSuccess;
}

}  // namespace webmlive
