// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "http_client/video_encoder.h"

#include <new>

#include "glog/logging.h"
#include "libyuv/convert.h"
#include "libyuv/planar_functions.h"

#if defined _MSC_VER
// Disable warning C4505(unreferenced local function has been removed) in MSVC.
// At the time this comment was written the warning is emitted 27 times for
// vp8.h and vp8cx.h (included by vpx_encoder.h).
#pragma warning(disable:4505)
#endif
#include "http_client/vpx_encoder.h"

namespace webmlive {

VideoFrame::VideoFrame()
    : keyframe_(false),
      width_(0),
      height_(0),
      stride_(0),
      timestamp_(0),
      duration_(0),
      buffer_capacity_(0),
      buffer_length_(0),
      format_(kVideoFormatI420) {
}

VideoFrame::~VideoFrame() {
}

int32 VideoFrame::Init(VideoFormat format,
                       bool keyframe,
                       int32 width,
                       int32 height,
                       int32 stride,
                       int64 timestamp,
                       int64 duration,
                       const uint8* ptr_data,
                       int32 data_length) {
  if (!ptr_data) {
    LOG(ERROR) << "VideoFrame can't Init with NULL data pointer.";
    return kInvalidArg;
  }

  const bool needs_conversion =
      (format != kVideoFormatI420 && format != kVideoFormatVP8);

  if (needs_conversion) {
    // Convert the video frame to I420.
    const int32 status = ConvertToI420(format, width, height, stride,
                                       ptr_data);
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
    format_ = format;
    width_ = width;
    height_ = height;
    stride_ = stride;
  }
  keyframe_ = keyframe;
  timestamp_ = timestamp;
  duration_ = duration;
  return kSuccess;
}

int32 VideoFrame::Clone(VideoFrame* ptr_frame) const {
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
  ptr_frame->format_ = format_;
  ptr_frame->keyframe_ = keyframe_;
  ptr_frame->width_ = width_;
  ptr_frame->height_ = height_;
  ptr_frame->stride_ = stride_;
  ptr_frame->timestamp_ = timestamp_;
  ptr_frame->duration_ = duration_;
  return kSuccess;
}

void VideoFrame::Swap(VideoFrame* ptr_frame) {
  CHECK_NOTNULL(buffer_.get());
  CHECK_NOTNULL(ptr_frame->buffer_.get());

  const VideoFormat temp_format = format_;
  format_ = ptr_frame->format_;
  ptr_frame->format_ = temp_format;

  const bool temp_keyframe = keyframe_;
  keyframe_ = ptr_frame->keyframe_;
  ptr_frame->keyframe_ = temp_keyframe;

  int32 temp = width_;
  width_ = ptr_frame->width_;
  ptr_frame->width_ = temp;

  temp = height_;
  height_ = ptr_frame->height_;
  ptr_frame->height_ = temp;

  temp = stride_;
  stride_ = ptr_frame->stride_;
  ptr_frame->stride_ = temp;

  int64 temp_time = timestamp_;
  timestamp_ = ptr_frame->timestamp_;
  ptr_frame->timestamp_ = temp_time;

  temp_time = duration_;
  duration_ = ptr_frame->duration_;
  ptr_frame->duration_ = temp_time;

  buffer_.swap(ptr_frame->buffer_);

  temp = buffer_capacity_;
  buffer_capacity_ = ptr_frame->buffer_capacity_;
  ptr_frame->buffer_capacity_ = temp;

  temp = buffer_length_;
  buffer_length_ = ptr_frame->buffer_length_;
  ptr_frame->buffer_length_ = temp;
}

int32 VideoFrame::ConvertToI420(VideoFormat format,
                                int32 width,
                                int32 height,
                                int32 source_stride,
                                const uint8* ptr_data) {
  // Allocate storage for the I420 frame.
  const int32 size_required = width * height * 3 / 2;
  if (size_required > buffer_capacity_) {
    buffer_.reset(new (std::nothrow) uint8[size_required]);  // NOLINT
    if (!buffer_) {
      LOG(ERROR) << "VideoFrame ConvertToI420 cannot allocate buffer.";
      return kNoMemory;
    }
    buffer_capacity_ = size_required;
  }
  buffer_length_ = size_required;

  format_ = kVideoFormatI420;
  width_ = width;
  height_ = height;
  stride_ = width;

  // Calculate length and stride for the I420 planes.
  const int32 y_length = width * height;
  const int32 uv_stride = stride_ / 2;
  const int32 uv_length = uv_stride * (height / 2);
  CHECK_EQ(buffer_length_, y_length + (uv_length * 2));

  // Assign the pointers to the I420 planes.
  uint8* ptr_i420_y = buffer_.get();
  uint8* ptr_i420_u = ptr_i420_y + y_length;
  uint8* ptr_i420_v = ptr_i420_u + uv_length;

  int status = kConversionFailed;
  switch (format) {
  case kVideoFormatYV12: {
    // Calculate length and stride for the YV12 planes.
    const int32 source_y_length = source_stride * height;
    const int32 source_uv_stride = source_stride / 2;
    const int32 source_uv_length = source_uv_stride * (height / 2);

    // Assign pointers to the YV12 V and U planes.
    const uint8* ptr_yv12_v = ptr_data + source_y_length;
    const uint8* ptr_yv12_u = ptr_yv12_v + source_uv_length;

    // Flip the U and V planes.
    status = libyuv::I420Copy(ptr_data,
                              source_stride,
                              ptr_yv12_v,
                              source_uv_stride,
                              ptr_yv12_u,
                              source_uv_stride,
                              ptr_i420_y,
                              stride_,
                              ptr_i420_u,
                              uv_stride,
                              ptr_i420_v,
                              uv_stride,
                              width,
                              height);
    break;
  }

  case kVideoFormatYUY2:
    status = libyuv::YUY2ToI420(ptr_data,
                                source_stride,
                                ptr_i420_y,
                                stride_,
                                ptr_i420_u,
                                uv_stride,
                                ptr_i420_v,
                                uv_stride,
                                width,
                                height);
    break;

  case kVideoFormatUYVY:
    status = libyuv::UYVYToI420(ptr_data,
                                source_stride,
                                ptr_i420_y,
                                stride_,
                                ptr_i420_u,
                                uv_stride,
                                ptr_i420_v,
                                uv_stride,
                                width,
                                height);
    break;

  case kVideoFormatBGR:
    status = libyuv::RGB24ToI420(ptr_data,
                                 source_stride,
                                 ptr_i420_y,
                                 stride_,
                                 ptr_i420_u,
                                 uv_stride,
                                 ptr_i420_v,
                                 uv_stride,
                                 width,
                                 height);
    break;

  case kVideoFormatBGRA:
    status = libyuv::BGRAToI420(ptr_data,
                                source_stride,
                                ptr_i420_y,
                                stride_,
                                ptr_i420_u,
                                uv_stride,
                                ptr_i420_v,
                                uv_stride,
                                width,
                                height);
    break;

  default:
    LOG(ERROR) << "Cannot convert to I420: invalid video format.";
    status = kInvalidArg;
  }

  return status;
}

///////////////////////////////////////////////////////////////////////////////
// VideoFrameQueue
//

VideoFrameQueue::VideoFrameQueue() {
}

VideoFrameQueue::~VideoFrameQueue() {
  boost::mutex::scoped_lock lock(mutex_);
  while (!frame_pool_.empty()) {
    delete frame_pool_.front();
    frame_pool_.pop();
  }
  while (!active_frames_.empty()) {
    delete active_frames_.front();
    active_frames_.pop();
  }
}

// Obtains lock and populates |frame_pool_| with |VideoFrame| pointers.
int32 VideoFrameQueue::Init() {
  boost::mutex::scoped_lock lock(mutex_);
  CHECK(frame_pool_.empty());
  CHECK(active_frames_.empty());
  for (int i = 0; i < kQueueLength; ++i) {
    VideoFrame* const ptr_frame = new (std::nothrow) VideoFrame;  // NOLINT
    if (!ptr_frame) {
      LOG(ERROR) << "VideoFrame allocation failed!";
      return kNoMemory;
    }
    frame_pool_.push(ptr_frame);
  }
  return kSuccess;
}

// Obtains lock, copies |ptr_frame| data into |VideoFrame| from |frame_pool_|,
// and moves the frame into |active_frames_|.
int32 VideoFrameQueue::Commit(VideoFrame* ptr_frame) {
  if (!ptr_frame || !ptr_frame->buffer()) {
    LOG(ERROR) << "VideoFrameQueue can't Commit a NULL/empty VideoFrame!";
    return kInvalidArg;
  }
  boost::mutex::scoped_lock lock(mutex_);
  if (frame_pool_.empty()) {
    VLOG(4) << "VideoFrameQueue full.";
    return kFull;
  }

  // Copy user data into front frame from |frame_pool_|.
  VideoFrame* const ptr_pool_frame = frame_pool_.front();
  if (ExchangeFrames(ptr_frame, ptr_pool_frame)) {
    LOG(ERROR) << "VideoFrame Commit ExchangeFrames failed!";
    return kNoMemory;
  }

  // Move the now active frame from the pool into the active queue.
  frame_pool_.pop();
  active_frames_.push(ptr_pool_frame);
  return kSuccess;
}

// Obtains lock, copies front |VideoFrame| from |active_frames_| to
// |ptr_frame|, and moves the consumed |VideoFrame| back into |frame_pool_|.
int32 VideoFrameQueue::Read(VideoFrame* ptr_frame) {
  if (!ptr_frame) {
    LOG(ERROR) << "VideoFrameQueue can't Read into a NULL VideoFrame!";
    return kInvalidArg;
  }
  boost::mutex::scoped_lock lock(mutex_);
  if (active_frames_.empty()) {
    VLOG(4) << "VideoFrameQueue empty.";
    return kEmpty;
  }

  // Copy active frame data to user frame.
  VideoFrame* const ptr_active_frame = active_frames_.front();
  if (ExchangeFrames(ptr_active_frame, ptr_frame)) {
    LOG(ERROR) << "VideoFrame Read ExchangeFrames failed!";
    return kNoMemory;
  }

  // Put the now inactive frame back in the pool.
  active_frames_.pop();
  frame_pool_.push(ptr_active_frame);
  return kSuccess;
}

// Obtains lock and drops any |VideoFrame|s in |active_frames_|.
void VideoFrameQueue::DropFrames() {
  boost::mutex::scoped_lock lock(mutex_);
  while (!active_frames_.empty()) {
    frame_pool_.push(active_frames_.front());
    active_frames_.pop();
  }
}

int32 VideoFrameQueue::ExchangeFrames(VideoFrame* ptr_source,
                                      VideoFrame* ptr_target) {
  if (!ptr_source || !ptr_target) {
    return kInvalidArg;
  }
  if (ptr_target->buffer()) {
    ptr_target->Swap(ptr_source);
  } else {
    const int32 status = ptr_source->Clone(ptr_target);
    if (status) {
      LOG(ERROR) << "VideoFrame Clone failed! " << status;
      return kNoMemory;
    }
  }
  return kSuccess;
}

///////////////////////////////////////////////////////////////////////////////
// VideoEncoder
//

VideoEncoder::VideoEncoder() {
}

VideoEncoder::~VideoEncoder() {
}

int32 VideoEncoder::Init(const WebmEncoderConfig& config) {
  ptr_vpx_encoder_.reset(new (std::nothrow) VpxEncoder());
  if (!ptr_vpx_encoder_) {
    return kNoMemory;
  }
  return ptr_vpx_encoder_->Init(config);
}

int32 VideoEncoder::EncodeFrame(const VideoFrame& raw_frame,
                                VideoFrame* ptr_vp8_frame) {
  if (!ptr_vpx_encoder_) {
    LOG(ERROR) << "VideoEncoder has NULL encoder, not Init'd";
    return kEncoderError;
  }
  return ptr_vpx_encoder_->EncodeFrame(raw_frame, ptr_vp8_frame);
}

///////////////////////////////////////////////////////////////////////////////
// VideoFrameCallbackInterface
//

VideoFrameCallbackInterface::~VideoFrameCallbackInterface() {
}


}  // namespace webmlive

