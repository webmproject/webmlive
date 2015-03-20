// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef WEBMLIVE_ENCODER_VPX_ENCODER_H_
#define WEBMLIVE_ENCODER_VPX_ENCODER_H_

#include "encoder/basictypes.h"
#include "encoder/encoder_base.h"
#include "encoder/video_encoder.h"

#define VPX_CODEC_DISABLE_COMPAT 1
#define VPX_DISABLE_CTRL_TYPECHECKS 1

#include "libvpx/vpx/vpx_encoder.h"
#include "libvpx/vpx/vp8cx.h"

namespace webmlive {
class VideoFrame;
struct WebmEncoderConfig;

// Simple wrapper class for VP8 encoding using libvpx.
class VpxEncoder {
 public:
  enum {
    // libvpx reported an error.
    kCodecError = VideoEncoder::kCodecError,
    // Error within |VpxEncoder|, but not reported by libvpx.
    kEncoderError = VideoEncoder::kEncoderError,
    kNoMemory = VideoEncoder::kNoMemory,
    kInvalidArg = VideoEncoder::kInvalidArg,
    kSuccess = VideoEncoder::kSuccess,
    // Frame dropped.
    kDropped = VideoEncoder::kDropped,
  };
  VpxEncoder();
  ~VpxEncoder();

  // Initializes libvpx for VPx encoding and returns |kSuccess|. Returns
  // |kCodecError| if a libvpx operation fails.
  int Init(const WebmEncoderConfig& config);

  // Encodes |ptr_raw_frame| using libvpx and returns the compressed data via
  // |ptr_vpx_frame|.
  // Return values:
  // |kSuccess| - frame encoded successfully.
  // |kDropped| - decimation is enabled and the frame stored in
  //              |ptr_raw_frame| was dropped.
  // |kCodecError| - a libvpx operation failed.
  // |kEncoderError| - compressed data cannot be stored in |ptr_vpx_frame|.
  int EncodeFrame(const VideoFrame& raw_frame, VideoFrame* ptr_vpx_frame);

  // Accessors.
  int64 frames_in() const { return frames_in_; }
  int64 frames_out() const { return frames_out_; }
  int64 last_keyframe_time() const { return last_keyframe_time_; }
  int64 last_timestamp() const { return last_timestamp_; }

 private:
  // Utility function for passing values to libvpx's vpx_codec_control
  // function. Does nothing and returns |kSuccess| when |val| is equal to
  // |default_val|. Returns |kEncoderError| when |control_id| is not
  // supported. Returns |kCodecError| when vpx_codec_control returns an error.
  template <typename T> int32 CodecControl(int control_id, T val,
                                           T default_val);

  // Number of raw frames passed to |EncodeFrame|.
  int64 frames_in_;

  // Number of compressed frames returned from |EncodeFrame|.
  int64 frames_out_;

  // Time of last keyframe reported by libvpx in |EncodeFrame|.
  int64 last_keyframe_time_;

  // Webmlive libvpx settings structure.
  VpxConfig config_;

  // libvpx VPx configuration structure.
  vpx_codec_ctx_t vpx_context_;

  // Timestamp of most recent compressed frame.
  int64 last_timestamp_;
  WEBMLIVE_DISALLOW_COPY_AND_ASSIGN(VpxEncoder);
};

}  // namespace webmlive

#endif  // WEBMLIVE_ENCODER_VPX_ENCODER_H_
