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
#include "libwebm/webmids.hpp"

namespace webmlive {

template <typename T>
T milliseconds_to_timecode_ticks(T milliseconds) {
  return milliseconds * LiveWebmMuxer::kTimecodeScale;
}

// Buffer object implementing libwebm's IMkvWriter interface. Constructed from
// user's |WebmChunkBuffer| to store data written by libwebm.
class WebmMuxWriter : public mkvmuxer::IMkvWriter {
 public:
  enum {
    kNotImplemented = -200,
    kInvalidArg = -1,
    kSuccess = 0,
  };
  explicit WebmMuxWriter(LiveWebmMuxer::WriteBuffer& buffer);
  virtual ~WebmMuxWriter();

  // mkvmuxer::IMkvWriter methods
  // Returns total bytes of data passed to |Write|.
  virtual int64 Position() const { return bytes_written_; }

  // Not seekable, return |kNotImplemented| on seek attempts.
  virtual int32 Position(int64) { return kNotImplemented; }

  // Always returns false: |WebmMuxWriter| is never seekable. Written data
  // goes into a vector, and data is buffered only until a chunk is completed.
  virtual bool Seekable() const { return false; }

  // Writes |ptr_buffer| contents to |buffer_|.
  virtual int32 Write(const void* ptr_buffer, uint32 buffer_length);

  // Called by libwebm, and notifies writer of element start position.
  virtual void ElementStartNotify(uint64 element_id, int64 position);

  // Resets |chunk_end_| to 0. Used by |LiveWebmMuxer| to
  void ResetChunkEnd() { chunk_end_ = 0; }

  // Accessors.
  int64 bytes_written() const { return bytes_written_; }
  int64 chunk_end() const { return chunk_end_; }

 private:
  int64 bytes_written_;
  int64 chunk_end_;
  LiveWebmMuxer::WriteBuffer& buffer_;
  WEBMLIVE_DISALLOW_COPY_AND_ASSIGN(WebmMuxWriter);
};

WebmMuxWriter::WebmMuxWriter(LiveWebmMuxer::WriteBuffer& buffer)
    : bytes_written_(0),
      chunk_end_(0),
      buffer_(buffer) {
}

WebmMuxWriter::~WebmMuxWriter() {
}

int32 WebmMuxWriter::Write(const void* ptr_buffer, uint32 buffer_length) {
  if (!ptr_buffer || !buffer_length) {
    LOG(ERROR) << "returning kInvalidArg to libwebm: NULL/0 length buffer.";
    return kInvalidArg;
  }
  const uint8* ptr_data = reinterpret_cast<const uint8*>(ptr_buffer);
  buffer_.insert(buffer_.end(), ptr_data, ptr_data + buffer_length);
  bytes_written_ += buffer_length;
  return kSuccess;
}

void WebmMuxWriter::ElementStartNotify(uint64 element_id, int64 position) {
  if (element_id == mkvmuxer::kMkvCluster) {
    chunk_end_ = bytes_written_ - position;
  }
}

///////////////////////////////////////////////////////////////////////////////
// LiveWebmMuxer
//

LiveWebmMuxer::LiveWebmMuxer() : audio_track_num_(0), video_track_num_(0) {
}

LiveWebmMuxer::~LiveWebmMuxer() {
}

int32 LiveWebmMuxer::Init(const AudioConfig *ptr_audio_config,
                          const VideoConfig *ptr_video_config,
                          int32 cluster_duration_milliseconds) {
  if (!ptr_audio_config && !ptr_video_config) {
    LOG(ERROR) << "muxer audio and video configs are NULL.";
    return kInvalidArg;
  }
  if (cluster_duration_milliseconds < 1) {
    LOG(ERROR) << "bad cluster duration, must be greater than 1 millisecond.";
    return kInvalidArg;
  }

  // Construct |WebmMuxWriter|-- it handles writes coming from libwebm.
  ptr_writer_.reset(new (std::nothrow) WebmMuxWriter(buffer_));
  if (!ptr_writer_) {
    LOG(ERROR) << "cannot construct WebmWriteBuffer.";
    return kNoMemory;
  }

  // Construct and Init |ptr_segment_|, then enable live mode.
  ptr_segment_.reset(new (std::nothrow) mkvmuxer::Segment());
  if (!ptr_segment_) {
    LOG(ERROR) << "cannot construct Segment.";
    return kNoMemory;
  }

  if (!ptr_segment_->Init(ptr_writer_.get())) {
    LOG(ERROR) << "cannot Init Segment.";
    return kMuxerError;
  }

  ptr_segment_->set_mode(mkvmuxer::Segment::kLive);
  const uint64 max_cluster_duration =
      milliseconds_to_timecode_ticks(cluster_duration_milliseconds);
  ptr_segment_->set_max_cluster_duration(max_cluster_duration);

  // Set segment info fields.
  using mkvmuxer::SegmentInfo;
  SegmentInfo* const ptr_segment_info = ptr_segment_->GetSegmentInfo();
  if (!ptr_segment_info) {
    LOG(ERROR) << "Segment has no SegmentInfo.";
    return kNoMemory;
  }
  ptr_segment_info->set_timecode_scale(kTimecodeScale);

  // TODO(tomfinegan): need constants for app and version
  ptr_segment_info->set_writing_app("webmlive v2");

  // Add the video track if user provided a non-NULL |ptr_video_config|
  if (ptr_video_config) {
    video_track_num_ = ptr_segment_->AddVideoTrack(ptr_video_config->width,
                                                   ptr_video_config->height);
    if (!video_track_num_) {
      LOG(ERROR) << "cannot AddVideoTrack on segment.";
      return kVideoTrackError;
    }
  }
  return kSuccess;
}

int32 LiveWebmMuxer::Finalize() {
  if (!ptr_segment_->Finalize()) {
    LOG(ERROR) << "libwebm mkvmuxer Finalize failed.";
    return kMuxerError;
  }
  return kSuccess;
}

int32 LiveWebmMuxer::WriteVideoFrame(const VideoFrame& vp8_frame) {
  if (!vp8_frame.buffer()) {
    LOG(ERROR) << "cannot write empty frame.";
    return kInvalidArg;
  }
  if (vp8_frame.format() != kVideoFormatVP8) {
    LOG(ERROR) << "cannot write non-VP8 frame.";
    return kInvalidArg;
  }
  if (!ptr_segment_->AddFrame(vp8_frame.buffer(),
                              vp8_frame.buffer_length(),
                              video_track_num_,
                              vp8_frame.timestamp(),
                              vp8_frame.keyframe())) {
    LOG(ERROR) << "AddFrame failed.";
    return kVideoWriteError;
  }
  return kSuccess;
}

// A chunk is ready when |WebmMuxWriter::chunk_length()| returns a value
// greater than 0.
bool LiveWebmMuxer::ChunkReady(int32* ptr_chunk_length) {
  if (ptr_chunk_length) {
    const int32 chunk_length = static_cast<int32>(ptr_writer_->chunk_end());
    if (chunk_length > 0) {
      *ptr_chunk_length = chunk_length;
      return true;
    }
  }
  return false;
}

// Copies the buffered chunk data into |ptr_buf|, erases it from |buffer_|, and
// calls |WebmMuxWriter::ResetChunkEnd()| to zero the chunk end position.
int32 LiveWebmMuxer::ReadChunk(uint8* ptr_buf, int32 buffer_capacity) {
  if (!ptr_buf) {
    LOG(ERROR) << "NULL buffer pointer";
    return kInvalidArg;
  }

  // Confirm user buffer is of adequate size.
  const int32 chunk_length = static_cast<int32>(ptr_writer_->chunk_end());
  if (buffer_capacity < chunk_length) {
    LOG(ERROR) << "not enough space for chunk";
    return kUserBufferTooSmall;
  }

  // Copy chunk to user buffer, and remove it from |buffer_|.
  memcpy(ptr_buf, &buffer_[0], chunk_length);
  WriteBuffer::iterator erase_end_pos = buffer_.begin() + chunk_length;
  buffer_.erase(buffer_.begin(), erase_end_pos);

  // Clear chunk end position, which causes |ChunkReady| to return false.
  ptr_writer_->ResetChunkEnd();
  return kSuccess;
}

}  // namespace webmlive
