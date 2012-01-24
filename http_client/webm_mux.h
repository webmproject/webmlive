// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef HTTP_CLIENT_WEBM_MUX_H_
#define HTTP_CLIENT_WEBM_MUX_H_

#include <vector>

#include "boost/scoped_ptr.hpp"
#include "http_client/basictypes.h"
#include "http_client/http_client_base.h"
#include "http_client/webm_encoder.h"

// Forward declarations of libwebm muxer types used by |LiveWebmMuxer|.
namespace mkvmuxer {
class Segment;
}

namespace webmlive {

// Forward declaration of class implementing IMkvWriter interface for libwebm.
class WebmMuxWriter;

// WebM muxing object built atop libwebm. Provides buffers containing WebM
// "chunks" of two types:
//  Metadata Chunk
//   Contains EBML header, segment info, and segment tracks elements.
//  Chunk
//   A complete WebM cluster element.
//
// Notes:
// - Only the first chunk written is metadata. All other chunks are clusters.
//
// - All element size values are set to unknown (an EBML encoded -1).
//
// - Users MUST call |Init| before any other method.
//
// - Users MUST call |Finalize| to avoid losing the final cluster; libwebm
//   must buffer data in some situations to satisfy WebM container guidelines:
//   http://www.webmproject.org/code/specs/container/
//
// - Users are responsible for keeping memory usage reasonable by calling
//   |ChunkReady| periodically-- when |ChunkReady| returns true, |ReadChunk|
//   will return the complete chunk and discard it from the buffer.
//
class LiveWebmMuxer {
 public:
  typedef std::vector<uint8> WriteBuffer;
  typedef WebmEncoderConfig::AudioCaptureConfig AudioConfig;
  typedef WebmEncoderConfig::VideoCaptureConfig VideoConfig;

  static const uint64 kTimecodeScale = 1000000;

  // Status codes returned by class methods.
  enum {
    // Temporary return code for unimplemented operations.
    kNotImplemented = -200,

    // |ReadChunk| called when no chunk is ready.
    kNoChunkReady = -7,

    // Buffer passed to |ReadChunk| was too small.
    kUserBufferTooSmall = -6,

    // Unable to write video frame.
    kVideoWriteError = -5,

    // Addition of the video track to |ptr_segment_| failed.
    kVideoTrackError = -4,

    // Something failed while interacting with the muxing library.
    kMuxerError = -3,

    kNoMemory = -2,
    kInvalidArg = -1,
    kSuccess = 0,
  };

  LiveWebmMuxer();
  ~LiveWebmMuxer();

  // Initializes libwebm for muxing in live mode, and adds tracks to
  // |ptr_segment_|. Passing a NULL configuration pointer disables the track of
  // that type. Returns |kSuccess| when successful. Returns |kInvalidArg| if
  // both configuration pointers are NULL. Returns |kInvalidArg| when
  // |cluster_duration| is < 1.
  int32 Init(const AudioConfig* ptr_audio_config,
             const VideoConfig* ptr_video_config,
             int32 cluster_duration_milliseconds);

  // Flushes any queued frames. Users MUST call this method to ensure that all
  // buffered frames are flushed out of libwebm. To determine if calling
  // |Finalize| resulted in production of a chunk, call |ChunkReady| after
  // the call to |Finalize|. Returns |kSuccess| when |Segment::Finalize|
  // returns without error.
  int32 Finalize();

  // Writes |vp8_frame| to the video track and returns |kSuccess|. Returns
  // |kInvalidArg| when |vp8_frame| is empty or contains a non-VP8 frame.
  // Returns |kVideoWriteError| when libwebm returns an error.
  int32 WriteVideoFrame(const VideoFrame& vp8_frame);

  // Returns true and writes chunk length to |ptr_chunk_length| when |buffer_|
  // contains a complete WebM chunk.
  bool ChunkReady(int32* ptr_chunk_length);

  // Moves WebM chunk data into |ptr_buf|. The data has been from removed from
  // |buffer_| when |kSuccess| is returned.  Returns |kUserBufferTooSmall| if
  // |buffer_capacity| is less than |chunk_length|.
  int32 ReadChunk(uint8* ptr_buf, int32 buffer_capacity);

 private:
  boost::scoped_ptr<WebmMuxWriter> ptr_writer_;
  boost::scoped_ptr<mkvmuxer::Segment> ptr_segment_;
  uint64 audio_track_num_;
  uint64 video_track_num_;
  WriteBuffer buffer_;
  friend class WebmMuxWriter;
  WEBMLIVE_DISALLOW_COPY_AND_ASSIGN(LiveWebmMuxer);
};

}  // namespace webmlive

#endif  // HTTP_CLIENT_WEBM_MUX_H_
