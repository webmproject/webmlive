// Copyright (c) 2012 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "client_encoder/webm_encoder.h"

#include <cstdio>
#include <sstream>

#include "client_encoder/buffer_pool-inl.h"
#include "client_encoder/webm_mux.h"
#ifdef _WIN32
#include "client_encoder/win/media_source_dshow.h"
#endif
#include "glog/logging.h"

namespace webmlive {

WebmEncoder::WebmEncoder()
    : initialized_(false),
      stop_(false),
      encoded_duration_(0) {
}

WebmEncoder::~WebmEncoder() {
}

// Constructs media source object and calls its |Init| method.
int WebmEncoder::Init(const WebmEncoderConfig& config,
                      DataSinkInterface* ptr_data_sink) {
  if (!ptr_data_sink) {
    LOG(ERROR) << "NULL data sink!";
    return kInvalidArg;
  }

  config_ = config;
  ptr_data_sink_ = ptr_data_sink;

  // Allocate the chunk buffer.
  chunk_buffer_.reset(
      new (std::nothrow) uint8[kDefaultChunkBufferSize]);  // NOLINT
  if (!chunk_buffer_) {
    LOG(ERROR) << "Unable to allocate chunk buffer!";
    return kNoMemory;
  }
  chunk_buffer_size_ = kDefaultChunkBufferSize;

  // Initialize the video frame queue.
  if (video_queue_.Init(false)) {
    LOG(ERROR) << "BufferPool<VideoFrame*> Init failed!";
    return kInitFailed;
  }

  // Construct and initialize the media source(s).
  ptr_media_source_.reset(new (std::nothrow) MediaSourceImpl());  // NOLINT
  if (!ptr_media_source_) {
    LOG(ERROR) << "cannot construct media source!";
    return kInitFailed;
  }
  int status = ptr_media_source_->Init(config_, this);
  if (status) {
    LOG(ERROR) << "media source Init failed " << status;
    return kInitFailed;
  }
  config_.actual_video_config = ptr_media_source_->actual_video_config();

  // Initialize the video encoder.
  status = video_encoder_.Init(config_);
  if (status) {
    LOG(ERROR) << "video encoder Init failed " << status;
    return kInitFailed;
  }

  // Construct and initialize the muxer.
  ptr_muxer_.reset(new (std::nothrow) LiveWebmMuxer());  // NOLINT
  if (!ptr_muxer_) {
    LOG(ERROR) << "cannot construct live muxer!";
    return kInitFailed;
  }
  status = ptr_muxer_->Init(config_.vpx_config.keyframe_interval);
  if (status) {
    LOG(ERROR) << "live muxer Init failed " << status;
    return kInitFailed;
  }

  // Add the video track.
  status = ptr_muxer_->AddTrack(config_.actual_video_config);
  if (status) {
    LOG(ERROR) << "live muxer AddTrack failed " << status;
    return kInitFailed;
  }

  initialized_ = true;
  return kSuccess;
}

int WebmEncoder::Run() {
  if (!initialized_) {
    LOG(ERROR) << "Encoder cannot Run, Init required.";
    return kRunFailed;
  }

  if (encode_thread_) {
    LOG(ERROR) << "non-null encode thread. Already running?";
    return kRunFailed;
  }

  using boost::bind;
  using boost::shared_ptr;
  using boost::thread;
  using std::nothrow;
  encode_thread_ = shared_ptr<thread>(
      new (nothrow) thread(bind(&WebmEncoder::EncoderThread,  // NOLINT
                                this)));

  return kSuccess;
}

// Sets |stop_| to true and calls join on |encode_thread_| to wait for
// |EncoderThread| to finish.
void WebmEncoder::Stop() {
  CHECK(encode_thread_);
  boost::mutex::scoped_lock lock(mutex_);
  stop_ = true;
  lock.unlock();
  encode_thread_->join();
}

// Returns encoded duration in seconds.
int64 WebmEncoder::encoded_duration() const {
  boost::mutex::scoped_lock lock(mutex_);
  return encoded_duration_;
}

// VideoFrameCallbackInterface
int WebmEncoder::OnVideoFrameReceived(VideoFrame* ptr_frame) {
  int status = video_queue_.Commit(ptr_frame);
  if (status) {
    if (status != BufferPool<VideoFrame>::kFull) {
      LOG(ERROR) << "VideoFrameQueue Push failed! " << status;
    }
    return VideoFrameCallbackInterface::kDropped;
  }
  LOG(INFO) << "OnVideoFrameReceived pushed frame.";
  return kSuccess;
}

// Tries to obtain lock on |mutex_| and returns value of |stop_| if lock is
// obtained. Assumes no stop requested and returns false if unable to obtain
// the lock.
bool WebmEncoder::StopRequested() {
  bool stop_requested = false;
  boost::mutex::scoped_try_lock lock(mutex_);
  if (lock.owns_lock()) {
    stop_requested = stop_;
  }
  return stop_requested;
}

bool WebmEncoder::ReadChunkFromMuxer(int32 chunk_length) {
  // Confirm that there's enough space in the chunk buffer.
  if (chunk_length > chunk_buffer_size_) {
    const int32 new_size = chunk_length * 2;
    chunk_buffer_.reset(new (std::nothrow) uint8[new_size]);  // NOLINT
    if (!chunk_buffer_) {
      LOG(ERROR) << "chunk buffer reallocation failed!";
      return false;
    }
    chunk_buffer_size_ = new_size;
  }

  // Read the chunk into |chunk_buffer_|.
  const int status = ptr_muxer_->ReadChunk(chunk_buffer_size_,
                                           chunk_buffer_.get());
  if (status) {
    LOG(ERROR) << "error reading chunk: " << status;
    return false;
  }

  return true;
}

void WebmEncoder::EncoderThread() {
  LOG(INFO) << "EncoderThread started.";

  // Set to true the encode loop breaks because |StopRequested()| returns true.
  bool user_initiated_stop = false;

  // Run the media source to get samples flowing.
  int status = ptr_media_source_->Run();

  if (status) {
    // media source Run failed; fatal
    LOG(ERROR) << "Unable to run the media source! " << status;
  } else {
    LiveWebmMuxer::WriteBuffer write_buffer;
    for (;;) {
      if (StopRequested()) {
        LOG(INFO) << "StopRequested returned true, stopping...";
        user_initiated_stop = true;
        break;
      }
      status = ptr_media_source_->CheckStatus();
      if (status) {
        LOG(ERROR) << "Media source in bad state, stopping... " << status;
        break;
      }

      // Read a frame for |video_queue_|. Sets |got_frame| to true if a frame
      // is available.
      bool got_frame = false;
      status = video_queue_.Decommit(&raw_frame_);
      if (status) {
        if (status != BufferPool<VideoFrame>::kEmpty) {
          // Really an error; not just an empty queue.
          LOG(ERROR) << "VideoFrameQueue Pop failed! " << status;
          break;
        } else {
          VLOG(4) << "No frames in VideoFrameQueue";
        }
      } else {
        LOG(INFO) << "Encoder thread read raw frame.";
        got_frame = true;
      }

      // Encode a video frame, and pass it to the muxer.
      if (got_frame) {
        status = video_encoder_.EncodeFrame(raw_frame_, &vp8_frame_);
        if (status) {
          LOG(ERROR) << "Video frame encode failed " << status;
          break;
        } else {
          // Update encoded duration if able to obtain the lock.
          boost::mutex::scoped_try_lock lock(mutex_);
          if (lock.owns_lock()) {
            encoded_duration_ = vp8_frame_.timestamp();
          }
        }
        status = ptr_muxer_->WriteVideoFrame(vp8_frame_);
        if (status) {
          LOG(ERROR) << "Video frame mux failed " << status;
          break;
        }
      }

      if (ptr_data_sink_->Ready()) {
        int32 chunk_length = 0;
        const bool chunk_ready = ptr_muxer_->ChunkReady(&chunk_length);

        if (chunk_ready) {
          // A complete chunk is waiting in |ptr_muxer_|'s buffer.
          if (!ReadChunkFromMuxer(chunk_length)) {
            LOG(ERROR) << "cannot read WebM chunk.";
            break;
          }

          // Pass the chunk to |ptr_data_sink_|.
          if (!ptr_data_sink_->WriteData(chunk_buffer_.get(), chunk_length)) {
            LOG(ERROR) << "data sink write failed!";
            break;
          }
        }
      }
    }

    if (user_initiated_stop) {
      // When |user_initiated_stop| is true the encode loop has been broken
      // cleanly (without error). Call |LiveWebmMuxer::Finalize()| to flush any
      // buffered samples, and upload the final chunk if one becomes available.
      status = ptr_muxer_->Finalize();

      if (status) {
        LOG(ERROR) << "muxer Finalize failed: " << status;
      } else {
        int32 chunk_length = 0;
        if (ptr_muxer_->ChunkReady(&chunk_length)) {
          LOG(INFO) << "mkvmuxer Finalize produced a chunk.";

          while (!ptr_data_sink_->Ready()) {
            boost::this_thread::sleep(boost::get_system_time() +
                                      boost::posix_time::milliseconds(1));
          }

          if (ReadChunkFromMuxer(chunk_length)) {
            const bool sink_write_ok =
                ptr_data_sink_->WriteData(chunk_buffer_.get(), chunk_length);
            if (!sink_write_ok) {
              LOG(ERROR) << "data sink write failed for final chunk!";
            } else {
              LOG(INFO) << "Final chunk upload initiated.";
            }
          }
        }
      }
    }

    ptr_media_source_->Stop();
  }
  LOG(INFO) << "EncoderThread finished.";
}

}  // namespace webmlive
