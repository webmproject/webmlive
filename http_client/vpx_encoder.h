// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef HTTP_CLIENT_VPX_ENCODER_H_
#define HTTP_CLIENT_VPX_ENCODER_H_

#include "http_client/basictypes.h"
#include "http_client/http_client_base.h"
#include "http_client/video_encoder.h"
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

  // Initializes libvpx for VP8 encoding and returns |kSuccess|. Returns
  // |kInvalidArg| when |ptr_config| is NULL. Returns |kCodecError| if a libvpx
  // operation fails.
  int32 Init(const WebmEncoderConfig* ptr_config);

  // Encodes |ptr_raw_frame| using libvpx and returns the compressed data via
  // |ptr_vp8_frame|.
  // Return values:
  // |kSuccess| - frame encoded successfully.
  // |kDropped| - decimation is enabled and the frame stored in
  //              |ptr_raw_frame| was dropped.
  // |kCodecError| - a libvpx operation failed.
  // |kEncoderError| - compressed data cannot be stored in |ptr_vp8_frame|.
  int32 EncodeFrame(const VideoFrame* const ptr_raw_frame,
                    VideoFrame* ptr_vp8_frame);
  int64 frames_in() { return frames_in_; }
  int64 frames_out() { return frames_out_; }
  int64 last_keyframe_time() { return last_keyframe_time_; }

 private:
  // Number of raw frames passed to |EncodeFrame|.
  int64 frames_in_;

  // Number of compressed frames returned from |EncodeFrame|.
  int64 frames_out_;

  // Time of last keyframe reported by libvpx in |EncodeFrame|.
  int64 last_keyframe_time_;

  // Webmlive libvpx settings structure.
  VpxConfig config_;

  // libvpx VP8 configuration structure.
  vpx_codec_ctx_t vp8_context_;
  WEBMLIVE_DISALLOW_COPY_AND_ASSIGN(VpxEncoder);
};

}  // namespace webmlive

#endif  // HTTP_CLIENT_VPX_ENCODER_H_
