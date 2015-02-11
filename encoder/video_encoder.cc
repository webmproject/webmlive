// Copyright (c) 2012 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "encoder/video_encoder.h"

#include <new>

#include "glog/logging.h"
#include "libyuv/convert.h"
#include "libyuv/planar_functions.h"
#include "libyuv/video_common.h"

#if defined _MSC_VER
// Disable warning C4505(unreferenced local function has been removed) in MSVC.
// At the time this comment was written the warning is emitted 27 times for
// vp8.h and vp8cx.h (included by vpx_encoder.h).
#pragma warning(disable:4505)
#endif
#include "encoder/vpx_encoder.h"

namespace webmlive {

bool FourCCToVideoFormat(uint32 fourcc,
                         uint16 bits_per_pixel,
                         VideoFormat* ptr_format) {
  bool converted = false;
  if (ptr_format) {
    switch (fourcc) {
      case 0:  // RGB formats.
        if (bits_per_pixel == 24) {
          *ptr_format = kVideoFormatRGB;
          converted = true;
        } else if (bits_per_pixel == 32) {
          *ptr_format = kVideoFormatRGBA;
          converted = true;
        }
        break;
      case libyuv::FOURCC_I420:
        if (bits_per_pixel == kI420BitCount) {
          *ptr_format = kVideoFormatI420;
          converted = true;
        }
        break;
      case libyuv::FOURCC_YV12:
        if (bits_per_pixel == kYV12BitCount) {
          *ptr_format = kVideoFormatYV12;
          converted = true;
        }
        break;
      case libyuv::FOURCC_YUY2:
        if (bits_per_pixel == kYUY2BitCount) {
          *ptr_format = kVideoFormatYUY2;
          converted = true;
        }
        break;
      case libyuv::FOURCC_YUYV:
        if (bits_per_pixel == kYUYVBitCount) {
          *ptr_format = kVideoFormatYUYV;
          converted = true;
        }
        break;
      case libyuv::FOURCC_UYVY:
        if (bits_per_pixel == kUYVYBitCount) {
          *ptr_format = kVideoFormatUYVY;
          converted = true;
        }
        break;
      default:
        LOG(WARNING) << "Unknown four char code.";
    }
  }
  return converted;
}

VideoFrame::VideoFrame()
    : keyframe_(false),
      timestamp_(0),
      duration_(0),
      buffer_capacity_(0),
      buffer_length_(0) {
}

VideoFrame::~VideoFrame() {
}

int VideoFrame::Init(const VideoConfig& config,
                     bool keyframe,
                     int64 timestamp,
                     int64 duration,
                     const uint8* ptr_data,
                     int32 data_length) {
  if (!ptr_data) {
    LOG(ERROR) << "VideoFrame can't Init with NULL data pointer.";
    return kInvalidArg;
  }

  const bool needs_conversion =
      (config.format != kVideoFormatI420 &&
       config.format != kVideoFormatYV12 &&
       config.format != kVideoFormatVP8);

  if (needs_conversion) {
    // Convert the video frame to I420.
    const int32 status = ConvertToI420(config, ptr_data);
    if (status) {
      LOG(ERROR) << "Video format conversion failed " << status;
      return status;
    }
  } else {
    // Data does not need conversion: copy directly into |buffer_|.
    if (data_length > buffer_capacity_) {
      buffer_.reset(new (std::nothrow) uint8[data_length]);  // NOLINT
      if (!buffer_) {
        LOG(ERROR) << "VideoFrame Init cannot allocate buffer.";
        return kNoMemory;
      }
      buffer_capacity_ = data_length;
    }
    memcpy(&buffer_[0], ptr_data, data_length);
    buffer_length_ = data_length;
    config_ = config;
  }
  keyframe_ = keyframe;
  timestamp_ = timestamp;
  duration_ = duration;
  return kSuccess;
}

int VideoFrame::Clone(VideoFrame* ptr_frame) const {
  if (!ptr_frame) {
    LOG(ERROR) << "cannot Clone to a NULL VideoFrame.";
    return kInvalidArg;
  }
  if (buffer_.get() && buffer_capacity_ > 0) {
    ptr_frame->buffer_.reset(
        new (std::nothrow) uint8[buffer_capacity_]);  // NOLINT
    if (!ptr_frame->buffer_) {
      LOG(ERROR) << "VideoFrame Clone cannot allocate buffer.";
      return kNoMemory;
    }
    memcpy(&ptr_frame->buffer_[0], &buffer_[0], buffer_length_);
  }
  ptr_frame->buffer_capacity_ = buffer_capacity_;
  ptr_frame->buffer_length_ = buffer_length_;
  ptr_frame->config_ = config_;
  ptr_frame->keyframe_ = keyframe_;
  ptr_frame->timestamp_ = timestamp_;
  ptr_frame->duration_ = duration_;
  return kSuccess;
}

void VideoFrame::Swap(VideoFrame* ptr_frame) {
  CHECK_NOTNULL(buffer_.get());
  CHECK_NOTNULL(ptr_frame->buffer_.get());

  const VideoConfig temp_config = config_;
  config_ = ptr_frame->config_;
  ptr_frame->config_ = temp_config;

  const bool temp_keyframe = keyframe_;
  keyframe_ = ptr_frame->keyframe_;
  ptr_frame->keyframe_ = temp_keyframe;

  int64 temp_time = timestamp_;
  timestamp_ = ptr_frame->timestamp_;
  ptr_frame->timestamp_ = temp_time;

  temp_time = duration_;
  duration_ = ptr_frame->duration_;
  ptr_frame->duration_ = temp_time;

  buffer_.swap(ptr_frame->buffer_);

  int32 temp = buffer_capacity_;
  buffer_capacity_ = ptr_frame->buffer_capacity_;
  ptr_frame->buffer_capacity_ = temp;

  temp = buffer_length_;
  buffer_length_ = ptr_frame->buffer_length_;
  ptr_frame->buffer_length_ = temp;
}

int VideoFrame::ConvertToI420(const VideoConfig& source_config,
                              const uint8* ptr_data) {
  // Allocate storage for the I420 frame.
  const int32 size_required =
      source_config.width * source_config.height * 3 / 2;
  if (size_required > buffer_capacity_) {
    buffer_.reset(new (std::nothrow) uint8[size_required]);  // NOLINT
    if (!buffer_) {
      LOG(ERROR) << "VideoFrame ConvertToI420 cannot allocate buffer.";
      return kNoMemory;
    }
    buffer_capacity_ = size_required;
  }
  buffer_length_ = size_required;

  VideoConfig& target_config = config_;
  target_config.format = kVideoFormatI420;
  target_config.width = source_config.width;
  target_config.height = abs(source_config.height);
  target_config.stride = source_config.width;

  // Calculate length and stride for the I420 planes.
  const int32 y_length = source_config.width * target_config.height;
  const int32 uv_stride = target_config.stride / 2;
  const int32 uv_length = uv_stride * (target_config.height / 2);
  CHECK_EQ(buffer_length_, y_length + (uv_length * 2));

  // Assign the pointers to the I420 planes.
  uint8* const ptr_i420_y = buffer_.get();
  uint8* const ptr_i420_u = ptr_i420_y + y_length;
  uint8* const ptr_i420_v = ptr_i420_u + uv_length;

  int status = kConversionFailed;
  switch (source_config.format) {
    case kVideoFormatYUY2:
    case kVideoFormatYUYV:
      status = libyuv::YUY2ToI420(ptr_data, source_config.stride,
                                  ptr_i420_y, target_config.stride,
                                  ptr_i420_u, uv_stride,
                                  ptr_i420_v, uv_stride,
                                  source_config.width, target_config.height);
      break;
    case kVideoFormatUYVY:
      status = libyuv::UYVYToI420(ptr_data, source_config.stride,
                                  ptr_i420_y, target_config.stride,
                                  ptr_i420_u, uv_stride,
                                  ptr_i420_v, uv_stride,
                                  source_config.width, target_config.height);
      break;

    // Note that RGB conversions always negate the height to ensure correct
    // image orientation.
    case kVideoFormatRGB:
      status = libyuv::RGB24ToI420(ptr_data, source_config.stride,
                                   ptr_i420_y, target_config.stride,
                                   ptr_i420_u, uv_stride,
                                   ptr_i420_v, uv_stride,
                                   source_config.width, -source_config.height);
      break;
    case kVideoFormatRGBA:
      status = libyuv::BGRAToI420(ptr_data, source_config.stride,
                                  ptr_i420_y, target_config.stride,
                                  ptr_i420_u, uv_stride,
                                  ptr_i420_v, uv_stride,
                                  source_config.width, -source_config.height);
      break;

    case kVideoFormatI420:
    case kVideoFormatVP8:
    case kVideoFormatYV12:
    case kVideoFormatCount:
      LOG(ERROR) << "Cannot convert to I420: invalid video format.";
      status = kInvalidArg;
  }

  return status;
}

///////////////////////////////////////////////////////////////////////////////
// VideoEncoder
//

VideoEncoder::VideoEncoder() {
}

VideoEncoder::~VideoEncoder() {
}

int VideoEncoder::Init(const WebmEncoderConfig& config) {
  ptr_vpx_encoder_.reset(new (std::nothrow) VpxEncoder());  // NOLINT
  if (!ptr_vpx_encoder_) {
    return kNoMemory;
  }
  return ptr_vpx_encoder_->Init(config);
}

int VideoEncoder::EncodeFrame(const VideoFrame& raw_frame,
                              VideoFrame* ptr_vp8_frame) {
  if (!ptr_vpx_encoder_) {
    LOG(ERROR) << "VideoEncoder has NULL encoder, not Init'd";
    return kEncoderError;
  }
  return ptr_vpx_encoder_->EncodeFrame(raw_frame, ptr_vp8_frame);
}

int64 VideoEncoder::frames_in() const {
  return ptr_vpx_encoder_ ? ptr_vpx_encoder_->frames_in() : 0;
}

int64 VideoEncoder::frames_out() const {
  return ptr_vpx_encoder_ ? ptr_vpx_encoder_->frames_out() : 0;
}

int64 VideoEncoder::last_keyframe_time() const {
  return ptr_vpx_encoder_ ? ptr_vpx_encoder_->last_keyframe_time() : 0;
}

int64 VideoEncoder::last_timestamp() const {
  return ptr_vpx_encoder_ ? ptr_vpx_encoder_->last_timestamp() : 0;
}

}  // namespace webmlive
