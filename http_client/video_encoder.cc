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

namespace webmlive {

VideoFrame::VideoFrame()
    : keyframe_(false),
      width_(0),
      height_(0),
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
                       int64 timestamp,
                       int64 duration,
                       const uint8* ptr_data,
                       int32 data_length) {
  if (!ptr_data) {
    LOG(ERROR) << "VideoFrame can't Init with NULL data pointer.";
    return kInvalidArg;
  }
  if (format != kVideoFormatI420 && format != kVideoFormatVP8) {
    LOG(ERROR) << "Unknown VideoFormat.";
    return kInvalidArg;
  }
  if (data_length > buffer_capacity_) {
    buffer_.reset(new (std::nothrow) uint8[data_length]);  // NOLINT(nothrow)
    if (!buffer_) {
      LOG(ERROR) << "VideoFrame Init cannot allocate buffer.";
      return kNoMemory;
    }
    buffer_capacity_ = data_length;
  }
  memcpy(&buffer_[0], ptr_data, data_length);
  buffer_length_ = data_length;
  format_ = format;
  keyframe_ = keyframe;
  width_ = width;
  height_ = height;
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
        new (std::nothrow) uint8[buffer_capacity_]);  // NOLINT(nothrow)
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

}  // namespace webmlive

