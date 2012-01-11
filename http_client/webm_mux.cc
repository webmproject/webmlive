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
#include "http_client/buffer_util.h"
#include "libwebm/mkvmuxer.hpp"

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

    // Not enough data in |write_buffer_| to satisfy |buffer_length| passed to
    // |Read|.
    kNotEnoughData = -2,
    kInvalidArg = -1,
    kSuccess = 0,
  };
  explicit WebmMuxWriter(WebmChunkBuffer& buffer);
  virtual ~WebmMuxWriter();

  // mkvmuxer::IMkvWriter methods
  // Returns total bytes of data passed to |Write|.
  virtual int64 Position() const { return bytes_written_; }

  // Not seekable, return |kNotImplemented| on seek attempts.
  virtual int32 Position(int64) { return kNotImplemented; }

  // Always returns false: |WebmMuxWriter| is never seekable. Written data
  // goes into a vector, and data is buffered only until a chunk is completed.
  virtual bool Seekable() const { return false; }

  // Writes |ptr_buffer| contents to |write_buffer_|.
  virtual int32 Write(const void* ptr_buffer, uint32 buffer_length);

 private:
  int64 bytes_written_;
  WebmChunkBuffer& write_buffer_;
  WEBMLIVE_DISALLOW_COPY_AND_ASSIGN(WebmMuxWriter);
};

WebmMuxWriter::WebmMuxWriter(WebmChunkBuffer& buffer)
    : bytes_written_(0),
      write_buffer_(buffer) {
}

WebmMuxWriter::~WebmMuxWriter() {
}

int32 WebmMuxWriter::Write(const void* ptr_buffer, uint32 buffer_length) {
  if (!ptr_buffer || !buffer_length) {
    LOG(ERROR) << "returning kInvalidArg to libwebm: NULL/0 length buffer.";
    return kInvalidArg;
  }
  const uint8* ptr_data = reinterpret_cast<const uint8*>(ptr_buffer);
  if (write_buffer_.BufferData(ptr_data, buffer_length) == kSuccess) {
    bytes_written_ += buffer_length;
  }
  return kSuccess;
}

///////////////////////////////////////////////////////////////////////////////
// LiveWebmMuxer
//

LiveWebmMuxer::LiveWebmMuxer() : audio_track_num_(0), video_track_num_(0) {
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

  // Construct |WebmChunkBuffer|-- it provides the write buffer for libwebm.
  ptr_buffer_.reset(new (std::nothrow) WebmChunkBuffer());
  if (!ptr_buffer_) {
    LOG(ERROR) << "cannot construct WebmChunkBuffer.";
    return kNoMemory;
  }

  // Construct |WebmMuxWriter|-- it handles writes coming from libwebm.
  ptr_writer_.reset(new (std::nothrow) WebmMuxWriter(*ptr_buffer_));
  if (!ptr_writer_) {
    LOG(ERROR) << "cannot construct WebmWriteBuffer.";
    return kNoMemory;
  }

  // Construct |Segment| and enable live mode.
  ptr_segment_.reset(new (std::nothrow) mkvmuxer::Segment(ptr_writer_.get()));
  if (!ptr_segment_) {
    LOG(ERROR) << "cannot construct Segment.";
    return kNoMemory;
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
  ptr_segment_info->set_writing_app("webmlive");

  // Add the video track if user provided a non-NULL |ptr_video_config|
  // TODO(tomfinegan): update libwebm-- there should be a track number arg here!
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

bool LiveWebmMuxer::ChunkReady(int32* ptr_chunk_length) {
  return ptr_buffer_->ChunkReady(ptr_chunk_length);
}

int32 LiveWebmMuxer::ReadChunk(uint8* ptr_buf, int32 length) {
  return ptr_buffer_->ReadChunk(ptr_buf, length);
}

}  // namespace webmlive
