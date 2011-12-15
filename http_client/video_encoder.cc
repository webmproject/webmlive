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
                       uint8* ptr_data,
                       int32 data_length) {
  if (!ptr_data) {
    LOG(ERROR) << "VideoFrame can't Init with NULL data pointer!";
    return kInvalidArg;
  }
  if (format != kVideoFormatI420 && format != kVideoFormatVP8) {
    LOG(ERROR) << "Unknown VideoFormat.";
    return kInvalidArg;
  }
  format_ = format;
  keyframe_ = keyframe;
  width_ = width;
  height_ = height;
  timestamp_ = timestamp;
  duration_ = duration;
  if (data_length != buffer_length_) {
    if (data_length > buffer_length_) {
      buffer_.reset(new (std::nothrow) uint8[data_length]);
      if (!buffer_) {
        return kNoMemory;
      }
    }
    buffer_length_ = data_length;
  }
  memcpy(&buffer_[0], ptr_data, data_length);
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

  temp = buffer_length_;
  buffer_length_ = ptr_frame->buffer_length_;
  ptr_frame->buffer_length_ = temp;
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
  DCHECK(frame_pool_.empty());
  DCHECK(active_frames_.empty());
  for (int i = 0; i < kMaxDepth; ++i) {
    VideoFrame* ptr_frame = new (std::nothrow) VideoFrame;
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
int32 VideoFrameQueue::Write(VideoFrame* ptr_frame) {
  if (!ptr_frame || !ptr_frame->buffer()) {
    LOG(ERROR) << "VideoFrameQueue can't Push a NULL/empty VideoFrame!";
    return kInvalidArg;
  }
  boost::mutex::scoped_lock lock(mutex_);
  if (frame_pool_.empty()) {
    VLOG(4) << "VideoFrameQueue full.";
    return kFull;
  }

  // Copy user data into front frame from |frame_pool_|.
  VideoFrame* ptr_pool_frame = frame_pool_.front();
  if (CopyFrame(ptr_frame, ptr_pool_frame)) {
    LOG(ERROR) << "VideoFrame CopyFrame failed!";
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
    LOG(ERROR) << "VideoFrameQueue can't Pop into a NULL VideoFrame!";
    return kInvalidArg;
  }
  boost::mutex::scoped_lock lock(mutex_);
  if (active_frames_.empty()) {
    VLOG(4) << "VideoFrameQueue empty.";
    return kEmpty;
  }

  // Copy active frame data to user frame.
  VideoFrame* ptr_active_frame = active_frames_.front();
  if (CopyFrame(ptr_active_frame, ptr_frame)) {
    LOG(ERROR) << "CopyFrame failed!";
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

int32 VideoFrameQueue::CopyFrame(VideoFrame* ptr_source,
                                 VideoFrame* ptr_target) {
  if (!ptr_source || !ptr_target) {
    return kInvalidArg;
  }
  if (ptr_target->buffer()) {
    ptr_target->Swap(ptr_source);
  } else {
    int status = ptr_target->Init(ptr_source->format(),
                                  ptr_source->keyframe(),
                                  ptr_source->width(),
                                  ptr_source->height(),
                                  ptr_source->timestamp(),
                                  ptr_source->duration(),
                                  ptr_source->buffer(),
                                  ptr_source->buffer_length());
    if (status) {
      LOG(ERROR) << "VideoFrame Init failed! " << status;
      return kNoMemory;
    }
  }
  return kSuccess;
}

///////////////////////////////////////////////////////////////////////////////
// VideoEncoder
//

}  // namespace webmlive

