// Copyright (c) 2012 The WebM project authors. All Rights Reserved.
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
    kNotInitialized = -2,
    kInvalidArg = -1,
    kSuccess = 0,
  };
  WebmMuxWriter();
  virtual ~WebmMuxWriter();

  // Stores |ptr_buffer| and returns |kSuccess|.
  int32 Init(LiveWebmMuxer::WriteBuffer* ptr_write_buffer);

  // Accessors.
  int64 bytes_written() const { return bytes_written_; }
  int64 chunk_end() const { return chunk_end_; }

  // Erases chunk from |ptr_write_buffer_|, resets |chunk_end_| to 0, and
  // updates |bytes_buffered_|.
  void EraseChunk();

  // mkvmuxer::IMkvWriter methods
  // Returns total bytes of data passed to |Write|.
  virtual int64 Position() const { return bytes_written_; }

  // Not seekable, return |kNotImplemented| on seek attempts.
  virtual int32 Position(int64) { return kNotImplemented; }

  // Always returns false: |WebmMuxWriter| is never seekable. Written data
  // goes into a vector, and data is buffered only until a chunk is completed.
  virtual bool Seekable() const { return false; }

  // Writes |ptr_buffer| contents to |ptr_write_buffer_|.
  virtual int32 Write(const void* ptr_buffer, uint32 buffer_length);

  // Called by libwebm, and notifies writer of element start position.
  virtual void ElementStartNotify(uint64 element_id, int64 position);

 private:
  int64 bytes_buffered_;
  int64 bytes_written_;
  int64 chunk_end_;
  LiveWebmMuxer::WriteBuffer* ptr_write_buffer_;
  WEBMLIVE_DISALLOW_COPY_AND_ASSIGN(WebmMuxWriter);
};

WebmMuxWriter::WebmMuxWriter()
    : bytes_buffered_(0),
      bytes_written_(0),
      chunk_end_(0),
      ptr_write_buffer_(NULL) {
}

WebmMuxWriter::~WebmMuxWriter() {
}

int32 WebmMuxWriter::Init(LiveWebmMuxer::WriteBuffer* ptr_write_buffer) {
  if (!ptr_write_buffer) {
    LOG(ERROR) << "Cannot Init, NULL write buffer.";
    return kInvalidArg;
  }
  ptr_write_buffer_ = ptr_write_buffer;
  return kSuccess;
}

void WebmMuxWriter::EraseChunk() {
  if (ptr_write_buffer_) {
    LiveWebmMuxer::WriteBuffer::iterator erase_end_pos =
        ptr_write_buffer_->begin() + static_cast<int32>(chunk_end_);
    ptr_write_buffer_->erase(ptr_write_buffer_->begin(), erase_end_pos);
    bytes_buffered_ = ptr_write_buffer_->size();
    chunk_end_ = 0;
  }
}

int32 WebmMuxWriter::Write(const void* ptr_buffer, uint32 buffer_length) {
  if (!ptr_write_buffer_) {
    LOG(ERROR) << "Cannot Write, not Initialized.";
    return kNotInitialized;
  }
  if (!ptr_buffer || !buffer_length) {
    LOG(ERROR) << "returning kInvalidArg to libwebm: NULL/0 length buffer.";
    return kInvalidArg;
  }
  const uint8* ptr_data = reinterpret_cast<const uint8*>(ptr_buffer);
  ptr_write_buffer_->insert(ptr_write_buffer_->end(),
                            ptr_data,
                            ptr_data + buffer_length);
  bytes_written_ += buffer_length;
  bytes_buffered_ = ptr_write_buffer_->size();
  return kSuccess;
}

void WebmMuxWriter::ElementStartNotify(uint64 element_id, int64) {
  if (element_id == mkvmuxer::kMkvCluster) {
    chunk_end_ = bytes_buffered_;
  }
}

///////////////////////////////////////////////////////////////////////////////
// LiveWebmMuxer
//

LiveWebmMuxer::LiveWebmMuxer() : audio_track_num_(0), video_track_num_(0) {
}

LiveWebmMuxer::~LiveWebmMuxer() {
}

int32 LiveWebmMuxer::Init(int32 cluster_duration_milliseconds) {
  if (cluster_duration_milliseconds < 1) {
    LOG(ERROR) << "bad cluster duration, must be greater than 1 millisecond.";
    return kInvalidArg;
  }

  // Construct and Init |WebmMuxWriter|-- it handles writes coming from libwebm.
  ptr_writer_.reset(new (std::nothrow) WebmMuxWriter());
  if (!ptr_writer_) {
    LOG(ERROR) << "cannot construct WebmWriteBuffer.";
    return kNoMemory;
  }
  if (ptr_writer_->Init(&buffer_)) {
   LOG(ERROR) << "cannot Init WebmWriteBuffer.";
    return kMuxerError;
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
  return kSuccess;
}

int32 LiveWebmMuxer::AddTrack(const AudioConfig&) {
  return kNotImplemented;
}

int32 LiveWebmMuxer::AddTrack(const VideoConfig& video_config) {
  if (video_track_num_ != 0) {
    LOG(ERROR) << "Cannot add video track: it already exists.";
    return kVideoTrackAlreadyExists;
  }
  video_track_num_ = ptr_segment_->AddVideoTrack(video_config.width,
                                                 video_config.height);
  if (!video_track_num_) {
    LOG(ERROR) << "cannot AddVideoTrack on segment.";
    return kVideoTrackError;
  }
  return kSuccess;
}

int32 LiveWebmMuxer::Finalize() {
  if (!ptr_segment_->Finalize()) {
    LOG(ERROR) << "libwebm mkvmuxer Finalize failed.";
    return kMuxerError;
  }

  if (buffer_.size() > 0) {
    // When data is in |buffer_| after the |mkvmuxer::Segment::Finalize()|
    // call, make the last chunk available to the user by forcing
    // |ChunkReady()| to return true one final time. This last chunk will
    // contain any data passed to |mkvmuxer::Segment::AddFrame()| since the
    // last call to |WebmMuxWriter::ElementStartNotify()|.
    ptr_writer_->ElementStartNotify(mkvmuxer::kMkvCluster,
                                    ptr_writer_->bytes_written());
  }

  return kSuccess;
}

int32 LiveWebmMuxer::WriteVideoFrame(const VideoFrame& vp8_frame) {
  if (video_track_num_ == 0) {
    LOG(ERROR) << "Cannot WriteVideoFrame without a video track.";
    return kNoVideoTrack;
  }
  if (!vp8_frame.buffer()) {
    LOG(ERROR) << "cannot write empty frame.";
    return kInvalidArg;
  }
  if (vp8_frame.format() != kVideoFormatVP8) {
    LOG(ERROR) << "cannot write non-VP8 frame.";
    return kInvalidArg;
  }
  const int64 timecode = milliseconds_to_timecode_ticks(vp8_frame.timestamp());
  if (!ptr_segment_->AddFrame(vp8_frame.buffer(),
                              vp8_frame.buffer_length(),
                              video_track_num_,
                              timecode,
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
int32 LiveWebmMuxer::ReadChunk(int32 buffer_capacity, uint8* ptr_buf) {
  if (!ptr_buf) {
    LOG(ERROR) << "NULL buffer pointer.";
    return kInvalidArg;
  }

  // Make sure there's a chunk ready.
  int32 chunk_length = 0;
  if (!ChunkReady(&chunk_length)) {
    LOG(ERROR) << "No chunk ready.";
    return kNoChunkReady;
  }

  // Confirm user buffer is of adequate size
  if (buffer_capacity < chunk_length) {
    LOG(ERROR) << "Not enough space for chunk.";
    return kUserBufferTooSmall;
  }

  LOG(INFO) << "ReadChunk capacity=" << buffer_capacity
            << " length=" << chunk_length
            << " total buffered=" << buffer_.size();

  // Copy chunk to user buffer, and erase it from |buffer_|.
  memcpy(ptr_buf, &buffer_[0], chunk_length);
  ptr_writer_->EraseChunk();
  return kSuccess;
}

}  // namespace webmlive
