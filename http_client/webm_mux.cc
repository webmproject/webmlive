// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "http_client/webm_mux.h"

#include <new>
#include <vector>

#include "glog/logging.h"
#include "libwebm/mkvmuxer.hpp"

namespace webmlive {

// Buffer object implementing libwebm's IMkvWriter interface. Uses a
// std::vector to store data written by libwebm for users. Users are
// responsible for keeping memory usage reasonable by calling the |DiscardData|
// method after calls to |ReadFromBuffer|.
class WebmMuxBuffer : public mkvmuxer::IMkvWriter {
 public:
  enum {
    kNotImplemented = -200,

    // Not enough data in |write_buffer_| to satisfy |buffer_length| passed to
    // |Read|.
    kNotEnoughData = -2,
    kInvalidArg = -1,
    kSuccess = 0,
  };
  WebmMuxBuffer();
  virtual ~WebmMuxBuffer();

  // Returns number of bytes available for reading from |write_buffer_|.
  int32 BytesAvailable() const { return write_buffer_.size(); }

  // Copies |num_bytes_to_read| bytes from |write_buffer_| to |ptr_buffer|.
  int32 Read(uint8* ptr_buffer, int32 buffer_length);

  // Erases |length| bytes from |write_buffer_|.
  int32 DiscardData(int32 length);

  // mkvmuxer::IMkvWriter methods
  // Returns total bytes of data passed to |Write|.
  virtual int64 Position() const { return bytes_written_; }

  // Not seekable, return |kNotImplemented| on seek attempts.
  virtual int32 Position(int64) { return kNotImplemented; }

  // Always returns false: |WebmMuxBuffer| is never seekable. Written data
  // goes into a vector, and data is buffered only until a chunk is completed.
  virtual bool Seekable() const { return false; }

  // Writes |ptr_buffer| contents to |write_buffer_|.
  virtual int32 Write(const void* ptr_buffer, uint32 buffer_length);

 private:
  int64 bytes_written_;
  std::vector<uint8> write_buffer_;
  WEBMLIVE_DISALLOW_COPY_AND_ASSIGN(WebmMuxBuffer);
};

WebmMuxBuffer::WebmMuxBuffer() : bytes_written_(0) {
}

WebmMuxBuffer::~WebmMuxBuffer() {
}

int32 WebmMuxBuffer::Read(uint8* ptr_buffer, int32 num_bytes_to_read) {
  if (!ptr_buffer) {
    LOG(ERROR) << "cannot Read into NULL buffer.";
    return kInvalidArg;
  }
  if (num_bytes_to_read > BytesAvailable()) {
    LOG(ERROR) << "Not enough data in buffer.";
    return kNotEnoughData;
  }
  memcpy(reinterpret_cast<void*>(ptr_buffer), &write_buffer_[0],
         num_bytes_to_read);
  return kSuccess;
}

int32 WebmMuxBuffer::DiscardData(int32 length) {
  if (length > BytesAvailable()) {
    LOG(ERROR) << "cannot discard length greater than total amount buffered.";
    return kNotEnoughData;
  }
  write_buffer_.erase(write_buffer_.begin(), write_buffer_.begin() + length);
  return kSuccess;
}

int32 WebmMuxBuffer::Write(const void* ptr_buffer, uint32 buffer_length) {
  if (!ptr_buffer || !buffer_length) {
    LOG(ERROR) << "returning kInvalidArg to libwebm: NULL/0 length buffer.";
    return kInvalidArg;
  }
  const uint8* ptr_data = reinterpret_cast<const uint8*>(ptr_buffer);
  for (uint32 i = 0; i < buffer_length; ++i) {
    write_buffer_.push_back(ptr_data[i]);
  }
  bytes_written_ += buffer_length;
  return kSuccess;
}

///////////////////////////////////////////////////////////////////////////////
// LiveWebmMuxer
//

LiveWebmMuxer::LiveWebmMuxer() : audio_track_num_(0), video_track_num_(0) {
}

int32 LiveWebmMuxer::Init(const AudioConfig *ptr_audio_config,
                          const VideoConfig *ptr_video_config) {
  if (!ptr_audio_config && !ptr_video_config) {
    LOG(ERROR) << "muxer cannot Init when aud and vid configs are NULL.";
    return kInvalidArg;
  }

  // Construct WebmMuxBuffer-- it handles writes coming from libwebm.
  ptr_buffer_.reset(new (std::nothrow) WebmMuxBuffer());
  if (!ptr_buffer_) {
    LOG(ERROR) << "muxer can't construct WebmWriteBuffer.";
    return kNoMemory;
  }

  // Construct and init the segment
  ptr_segment_.reset(new (std::nothrow) mkvmuxer::Segment(ptr_buffer_.get()));
  if (!ptr_segment_) {
    LOG(ERROR) << "muxer can't construct Segment.";
    return kNoMemory;
  }
  using mkvmuxer::SegmentInfo;
  SegmentInfo* const ptr_segment_info = ptr_segment_->GetSegmentInfo();
  if (!ptr_segment_info) {
    LOG(ERROR) << "Segment has no SegmentInfo.";
    return kNoMemory;
  }
  ptr_segment_info->set_timecode_scale(kTimecodeScale);
  ptr_segment_info->set_writing_app("webmlive");

  // Add the video track if user provided a non-NULL |ptr_video_config|
  // TODO(tomfinegan): update libwebm-- there should be a track number arg here!
  if (ptr_video_config) {
    ptr_segment_->AddVideoTrack(ptr_video_config->width,
                                ptr_video_config->height);
  }
  return kSuccess;
}

}  // namespace webmlive
