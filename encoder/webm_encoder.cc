// Copyright (c) 2012 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "encoder/webm_encoder.h"

#include <algorithm>
#include <chrono>
#include <functional>
#include <cstdio>
#include <cstdlib>
#include <sstream>

#include "encoder/buffer_pool-inl.h"
#include "encoder/dash_writer.h"
#include "encoder/webm_mux.h"
#ifdef _WIN32
#include "encoder/win/media_source_dshow.h"
#endif
#include "glog/logging.h"

namespace {

const char kMuxedId[] = "muxed";
const char kAudioId[] = "audio";
const char kVideoId[] = "video";

// Adds |timestamp_offset| to the timestamp value of |ptr_sample|, and returns
// |WebmEncoder::kSuccess|. Returns |WebmEncoder::kInvalidArg| when |ptr_sample|
// is NULL.
template <class T>
int OffsetTimestamp(int64 timestamp_offset, T* ptr_sample) {
  using webmlive::WebmEncoder;
  if (!ptr_sample) {
    LOG(ERROR) << "cannot offset the timestamp of a NULL sample.";
    return WebmEncoder::kInvalidArg;
  }
  const int64 new_ts = timestamp_offset + ptr_sample->timestamp();
  ptr_sample->set_timestamp(new_ts);
  return WebmEncoder::kSuccess;
}

int InitMuxer(int chunk_duration, const std::string& muxer_id,
              std::unique_ptr<webmlive::LiveWebmMuxer>* muxer) {
  CHECK_NOTNULL(muxer);
  (*muxer).reset(new (std::nothrow) webmlive::LiveWebmMuxer());  // NOLINT
  if (!(*muxer).get()) {
    LOG(ERROR) << "cannot construct live muxer!";
    return webmlive::WebmEncoder::kInitFailed;
  }
  const int status = (*muxer)->Init(chunk_duration, muxer_id);
  if (status) {
    LOG(ERROR) << "live muxer Init failed " << status;
    return webmlive::WebmEncoder::kInitFailed;
  }
  return status;
}

bool WriteManifest(const std::string& name, const std::string& manifest) {
  FILE* manifest_file = fopen(name.c_str(), "w");
  if (!manifest_file) {
    LOG(ERROR) << "Unable to open manifest file.";
    return false;
  }
  const int bytes_written =
    fprintf(manifest_file, "%s", manifest.c_str());
  fclose(manifest_file);
  return (bytes_written == manifest.length());
}

bool WriteChunkFile(const std::string& chunk_name,
                    const uint8* chunk_buffer, int32 chunk_length) {
  FILE* chunk_file = fopen(chunk_name.c_str(), "wb");
  if (!chunk_file) {
    LOG(ERROR) << "Unable to open chunk file.";
    return false;
  }
  const size_t bytes_written =
      fwrite(reinterpret_cast<const void*>(chunk_buffer),
             1, chunk_length, chunk_file);
  fclose(chunk_file);
  return (bytes_written == chunk_length);
}

}  // anonymous namespace

namespace webmlive {

WebmEncoder::WebmEncoder()
    : initialized_(false),
      stop_(false),
      chunk_buffer_size_(0),
      encoded_duration_(0),
      ptr_encode_func_(NULL),
      timestamp_offset_(0) {
}

WebmEncoder::~WebmEncoder() {
}

// Constructs media source object and calls its |Init| method.
int WebmEncoder::Init(const WebmEncoderConfig& config,
                      DataSinkInterface* ptr_data_sink) {
  if (config.disable_audio && config.disable_video) {
    LOG(ERROR) << "Audio and video are disabled!";
    return kInvalidArg;
  }
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

  // Construct and initialize the media source(s).
  ptr_media_source_.reset(new (std::nothrow) MediaSourceImpl());  // NOLINT
  if (!ptr_media_source_) {
    LOG(ERROR) << "cannot construct media source!";
    return kInitFailed;
  }
  int status = ptr_media_source_->Init(config_, this, this);
  if (status) {
    LOG(ERROR) << "media source Init failed " << status;
    return kInitFailed;
  }

  // TODO(tomfinegan): Obey the command line instead of hard coding DASH output.
  config_.dash_encode = true;

  // When doing a DASH encode two muxers are used: One for each stream.
  // Otherwise there's only one. Configure the muxers via local pointers-- the
  // muxer actually being configured isn't really a concern of the code below as
  // long as the configuration attempt succeeds.
  LiveWebmMuxer* audio_muxer = NULL;
  LiveWebmMuxer* video_muxer = NULL;

  // Construct and initialize the muxer(s).
  if (config_.dash_encode) {
    status = InitMuxer(config_.vpx_config.keyframe_interval, kAudioId,
                       &ptr_muxer_aud_);
    if (status) {
      LOG(ERROR) << "InitMuxer (A) failed: " << status;
      return status;
    }
    status = InitMuxer(0, kVideoId, &ptr_muxer_vid_);
    if (status) {
      LOG(ERROR) << "InitMuxer (V) failed: " << status;
      return status;
    }
    audio_muxer = ptr_muxer_aud_.get();
    video_muxer = ptr_muxer_vid_.get();
  } else {
    status = InitMuxer(0, kMuxedId, &ptr_muxer_);
    if (status) {
      LOG(ERROR) << "InitMuxer failed: " << status;
      return status;
    }
    audio_muxer = ptr_muxer_.get();
    video_muxer = ptr_muxer_.get();
  }

  if (config_.disable_video == false) {
    config_.actual_video_config = ptr_media_source_->actual_video_config();

    // Initialize the video frame pool.
    const int default_count = BufferPool<VideoFrame>::kDefaultBufferCount;
    const double& fps = config_.actual_video_config.frame_rate;

    // Buffer up to half a second of video when audio is enabled.
    // TODO(tomfinegan): Add a VPx frame pool to store compressed frames while
    //                   waiting for audio instead of throwing memory at the
    //                   problem.
    const int num_video_buffers =
        config_.disable_audio ? default_count : static_cast<int>(fps / 2.0);
    if (video_pool_.Init(false, num_video_buffers)) {
      LOG(ERROR) << "BufferPool<VideoFrame> Init failed!";
      return kInitFailed;
    }

    // Initialize the video encoder.
    status = video_encoder_.Init(config_);
    if (status) {
      LOG(ERROR) << "video encoder Init failed " << status;
      return kInitFailed;
    }

    // Add the video track.
    VideoConfig vpx_video_config = config_.actual_video_config;
    vpx_video_config.format = config_.vpx_config.codec;
    status = video_muxer->AddTrack(vpx_video_config);
    if (status) {
      LOG(ERROR) << "live muxer AddTrack(video) failed " << status;
      return kInitFailed;
    }
  }

  if (config_.disable_audio == false) {
    config_.actual_audio_config = ptr_media_source_->actual_audio_config();

    // Initialize the audio buffer pool.
    const int num_audio_buffers = BufferPool<AudioBuffer>::kDefaultBufferCount;
    if (audio_pool_.Init(true, num_audio_buffers)) {
      LOG(ERROR) << "BufferPool<AudioBuffer> Init failed!";
      return kInitFailed;
    }

    // Initialize the vorbis encoder.
    status = vorbis_encoder_.Init(config_.actual_audio_config,
                                  config_.vorbis_config);
    if (status) {
      LOG(ERROR) << "audio encoder Init failed " << status;
      return kInitFailed;
    }

    // Fill in the private data structure.
    VorbisCodecPrivate codec_private;
    codec_private.ptr_ident = vorbis_encoder_.ident_header();
    codec_private.ident_length = vorbis_encoder_.ident_header_length();
    codec_private.ptr_comments = vorbis_encoder_.comments_header();
    codec_private.comments_length = vorbis_encoder_.comments_header_length();
    codec_private.ptr_setup = vorbis_encoder_.setup_header();
    codec_private.setup_length = vorbis_encoder_.setup_header_length();

    // Add the vorbis track.
    status = audio_muxer->AddTrack(config_.actual_audio_config, codec_private);
    if (status) {
      LOG(ERROR) << "live muxer AddTrack(audio) failed " << status;
      return kInitFailed;
    }
  }

  if (config_.dash_encode) {
    ptr_encode_func_ = &WebmEncoder::DashEncode;
  }else if (config_.disable_audio) {
    ptr_encode_func_ = &WebmEncoder::EncodeVideoFrame;
  } else if (config_.disable_video) {
    ptr_encode_func_ = &WebmEncoder::EncodeAudioOnly;
  } else {
    ptr_encode_func_ = &WebmEncoder::AVEncode;
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

  using std::bind;
  using std::shared_ptr;
  using std::thread;
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
  mutex_.lock();
  stop_ = true;
  mutex_.unlock();
  encode_thread_->join();
}

// Returns encoded duration in seconds.
int64 WebmEncoder::encoded_duration() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return encoded_duration_;
}

// AudioSamplesCallbackInterface
int WebmEncoder::OnSamplesReceived(AudioBuffer* ptr_buffer) {
  const int status = audio_pool_.Commit(ptr_buffer);
  if (status) {
    LOG(ERROR) << "AudioBuffer pool Commit failed! " << status;
    return AudioSamplesCallbackInterface::kNoMemory;
  }
  LOG(INFO) << "OnSamplesReceived committed an audio buffer.";
  return kSuccess;
}

// VideoFrameCallbackInterface
int WebmEncoder::OnVideoFrameReceived(VideoFrame* ptr_frame) {
  const int status = video_pool_.Commit(ptr_frame);
  if (status) {
    if (status != BufferPool<VideoFrame>::kFull) {
      LOG(ERROR) << "VideoFrame pool Commit failed: " << status;
    }
    LOG(INFO) << "VideoFrame pool dropped frame (no buffers).";
    return VideoFrameCallbackInterface::kDropped;
  }
  LOG(INFO) << "OnVideoFrameReceived committed a frame.";
  return kSuccess;
}

// Tries to obtain lock on |mutex_| and returns value of |stop_| if lock is
// obtained. Assumes no stop requested and returns false if unable to obtain
// the lock.
bool WebmEncoder::StopRequested() {
  bool stop_requested = false;
  std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
  if (lock.owns_lock()) {
    stop_requested = stop_;
  }
  return stop_requested;
}

bool WebmEncoder::ReadChunkFromMuxer(std::unique_ptr<LiveWebmMuxer>* muxer,
                                     int32 chunk_length) {
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
  const int status = (*muxer)->ReadChunk(chunk_buffer_size_,
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

  if (!ptr_encode_func_) {
    // NULL encode function pointer; fatal/die:
    LOG(FATAL) << "NULL encode function pointer!";
  }

  // Run the media source to get samples flowing.
  int status = ptr_media_source_->Run();
  if (status) {
    // media source Run failed; fatal/die:
    LOG(FATAL) << "Unable to run the media source! " << status;
  }

  // Send the DASH manifest.
  dash_writer_.reset(new (std::nothrow) DashWriter);  // NOLINT
  if (!dash_writer_) {
    LOG(FATAL) << "cannot construct dash writer!";
  }
  if (!dash_writer_->Init(config_)) {
    LOG(ERROR) << "DashWriter::Init failed.";
  }
  std::string dash_manifest;
  if (!dash_writer_->WriteManifest(&dash_manifest)) {
    LOG(ERROR) << "DashWriter::WriteManifest failed.";
  }

#if 0
  ptr_data_sink_->WriteData(
      reinterpret_cast<const uint8*>(dash_manifest.data()),
      dash_manifest.length(), "manifest");
#endif

  // HACK: HERE BE DRAGONS
  CHECK(WriteManifest(config_.dash_dir + "webmlive.mpd", dash_manifest));

  // Wait for an input sample from each input stream-- this sets the
  // |timestamp_offset_| value when one or both streams starts with a negative
  // timestamp to avoid passing negative timestamps to libvpx and libwebm.
  status = WaitForSamples();
  if (status) {
    LOG(ERROR) << "WaitForSamples failed: " << status;
  } else {
    for (;;) {
      if (StopRequested()) {
        LOG(INFO) << "StopRequested returned true, stopping...";
        user_initiated_stop = true;
        break;
      }
      status = ptr_media_source_->CheckStatus();
      if (status) {
        LOG(ERROR) << "Media source in a bad state, stopping: " << status;
        break;
      }
      status = (this->*ptr_encode_func_)();
      if (status) {
        LOG(ERROR) << "encoding failed: " << status;
        break;
      }
      if (config_.dash_encode) {
        if (!config_.disable_audio) {
          status = WriteMuxerChunkToDataSink(&ptr_muxer_aud_);
          if (status) {
            LOG(ERROR) << "chunk write (A) failed: " << status;
            break;
          }
        }
        if (!config_.disable_video) {
          status = WriteMuxerChunkToDataSink(&ptr_muxer_vid_);
          if (status) {
            LOG(ERROR) << "chunk write (V) failed: " << status;
            break;
          }
        }
      } else {
        status = WriteMuxerChunkToDataSink(&ptr_muxer_);
        if (status) {
          LOG(ERROR) << "muxed chunk write failed: " << status;
          break;
        }
      }
    }

    if (user_initiated_stop) {
      // When |user_initiated_stop| is true the encode loop has been broken
      // cleanly (without error). Call |LiveWebmMuxer::Finalize()| to flush any
      // buffered samples, and upload the final chunk if one becomes available.
      if (config_.dash_encode) {
        if (!config_.disable_audio) {
          status = WriteLastMuxerChunkToDataSink(&ptr_muxer_aud_);
          if (status) {
            LOG(ERROR) << "Failed to write last dash audio chunk";
          }
        }
        if (!config_.disable_video) {
          status = WriteLastMuxerChunkToDataSink(&ptr_muxer_vid_);
          if (status) {
            LOG(ERROR) << "Failed to write last dash video chunk";
          }
        }
      } else {
        status = WriteLastMuxerChunkToDataSink(&ptr_muxer_);
        if (status) {
          LOG(ERROR) << "Failed to write last non-dash chunk";
        }
      }
    }

    ptr_media_source_->Stop();
  }
  LOG(INFO) << "EncoderThread finished.";
}

// On each encoding pass:
// - Attempts to read one uncompressed audio buffer from |audio_pool_|, and
//   feeds it |vorbis_encoder_| for compression when successful.
// - Passes all available compressed audio produced by |vorbis_encoder_| to
//   |ptr_muxer_| for muxing.
int WebmEncoder::EncodeAudioOnly() {
  // Encode a single audio buffer.
  int status = EncodeAudioBuffer();
  if (status) {
    LOG(ERROR) << "EncodeAudioBuffer failed: " << status;
    return status;
  }

  // Read and mux vorbis data until no more is available from |vorbis_encoder_|.
  AudioBuffer* vb = &vorbis_audio_buffer_;
  VorbisEncoder* ve = &vorbis_encoder_;
  while ((status = ve->ReadCompressedAudio(vb)) == kSuccess) {
    // Mux the vorbis data.
    const int mux_status = ptr_muxer_->WriteAudioBuffer(*vb);
    if (mux_status) {
      LOG(ERROR) << "Audio buffer mux failed " << mux_status;
      return mux_status;
    }
    VLOG(4) << "muxed (A) " << vorbis_audio_buffer_.timestamp() / 1000.0;

    // Update encoded duration if able to obtain the lock.
    std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
    if (lock.owns_lock()) {
      encoded_duration_ = vb->timestamp();
    }
  }
  if (status < 0) {
    LOG(ERROR) << "Error reading vorbis samples: " << status;
    return kAudioEncoderError;
  }
  return kSuccess;
}

// On each encoding pass:
// - Attempts to read an uncompressed audio buffer from |audio_pool_|, and
//   passes it to |vorbis_encoder_| when a buffer is available.
// - Stores the timestamp of the first available video frame from |video_pool_|
//   in |video_timestamp|, or uses the last encoded timestamp added to the
//   calculated time per frame if no frame is available.
// - Reads one compressed audio buffer from |vorbis_encoder_| into
//   |vorbis_audio_buffer_|, and
//   - Passes it to |ptr_muxer_| when the compressed audio buffer timestamp is
//     less than the stored video timestamp, or
//   - Stores the compressed audio buffer and sets the |vorbis_buffered| flag
//     to true, and then waits to mux the audio until:
// - When the stored |video_timestamp| is less than or equal to the _estimated_
//   timestamp of the next compressed audio buffer from |vorbis_encoder_|, calls
//   |EncodeVideoFrame()| to attempt to read and encode a video frame. This is
//   repeated until no video frames are available, or the next frame available
//   would cause video to get ahead of audio.
// - When the |vorbis_buffered| flag was set because the audio timestamp
//   produced by |vorbis_encoder_| was greater than |video_timestamp|, passes
//   |vorbis_audio_buffer_| to |ptr_muxer_|.
int WebmEncoder::AVEncode() {
  // Encode a single audio buffer.
  int status = EncodeAudioBuffer();
  if (status) {
    LOG(ERROR) << "EncodeAudioBuffer failed: " << status;
    return status;
  }

  // Store the next video timestamp.
  int64 video_timestamp = 0;
  status = PeekVideoTimestamp(&video_timestamp);
  if (status < 0) {
    LOG(ERROR) << "Video timestamp peek failed: " << status;
    return status;
  }

  // Read compressed audio until no more remains, or the compressed buffer
  // timestamp is greater than |video_timestamp|.
  bool vorbis_buffered = false;
  AudioBuffer& vorb_buf = vorbis_audio_buffer_;
  VorbisEncoder& vorb_enc = vorbis_encoder_;
  if (vorb_enc.time_encoded() <= video_timestamp) {
    while ((status = vorb_enc.ReadCompressedAudio(&vorb_buf)) == kSuccess) {
      if (video_timestamp < vorb_buf.timestamp()) {
        vorbis_buffered = true;
        break;
      }
      status = ptr_muxer_->WriteAudioBuffer(vorb_buf);
      if (status) {
        LOG(ERROR) << "audio mux failed: " << status;
        return status;
      }
      vorbis_buffered = false;
      VLOG(4) << "muxed (A) " << vorbis_audio_buffer_.timestamp() / 1000.0;
    }
  }

  // Attempt to encode video frames when |video_timestamp| is less than the
  // next estimated compressed audio buffer timestamp.
  while (status != BufferPool<VideoFrame>::kEmpty &&
         video_timestamp <= vorb_enc.time_encoded()) {
    VLOG(3) << "attempting video mux vid_ts=" << video_timestamp
              << " vorb_enc time_encoded=" << vorb_enc.time_encoded();
    status = EncodeVideoFrame();
    if (status) {
      LOG(ERROR) << "EncodeVideoFrame failed: " << status;
      return status;
    }
    status = PeekVideoTimestamp(&video_timestamp);
    if (status < 0) {
      LOG(ERROR) << "Video timestamp peek failed: " << status;
      return status;
    }
  }

  // TODO(tomfinegan): Update libwebm to handle non-monotonic timestamps on
  //                   audio by buffering samples in some way, and get rid
  //                   of this extra bit of audio handling code.

  // Mux compressed audio stored because its buffer timestamp was greater than
  // |video_timestamp|.
  if (vorbis_buffered) {
    status = ptr_muxer_->WriteAudioBuffer(vorb_buf);
    if (status) {
      LOG(ERROR) << "buffered audio mux failed: " << status;
      return status;
    }
    VLOG(4) << "muxed (buf, A) " << vorbis_audio_buffer_.timestamp() / 1000.0;
  }
  return kSuccess;
}

int WebmEncoder::DashEncode() {
  // Encode a single audio buffer.
  int status = EncodeAudioBuffer();
  if (status) {
    LOG(ERROR) << "EncodeAudioBuffer failed: " << status;
    return status;
  }

  int64 video_timestamp;
  status = PeekVideoTimestamp(&video_timestamp);
  if (status < 0) {
    LOG(ERROR) << "Video timestamp peek failed: " << status;
    return status;
  }

  // Read compressed audio until no more remains, or the compressed buffer
  // timestamp is greater than |video_timestamp|.
  bool vorbis_buffered = false;
  AudioBuffer& vorb_buf = vorbis_audio_buffer_;
  VorbisEncoder& vorb_enc = vorbis_encoder_;
  while ((status = vorb_enc.ReadCompressedAudio(&vorb_buf)) == kSuccess) {
    status = ptr_muxer_aud_->WriteAudioBuffer(vorb_buf);
    if (status) {
      LOG(ERROR) << "audio mux failed: " << status;
      return status;
    }
    VLOG(4) << "muxed (A) " << vorbis_audio_buffer_.timestamp() / 1000.0;

    // Keep stream time reasonably close.
    if (vorb_enc.time_encoded() > video_timestamp)
      break;
  }

  // Attempt to encode video frames when |video_timestamp| is less than the
  // next estimated compressed audio buffer timestamp.
  while (status != BufferPool<VideoFrame>::kEmpty) {
    VLOG(3) << "attempting video mux vid_ts=" << video_timestamp
            << " vorb_enc time_encoded=" << vorb_enc.time_encoded();
    status = EncodeVideoFrame();
    if (status) {
      LOG(ERROR) << "EncodeVideoFrame failed: " << status;
      return status;
    }
    status = PeekVideoTimestamp(&video_timestamp);
    if (status < 0) {
      LOG(ERROR) << "Video timestamp peek failed: " << status;
      return status;
    }

    // Keep stream time reasonably close.
    if (video_timestamp > vorb_enc.time_encoded())
      break;
  }
  return kSuccess;
}


// Reads, compresses and muxes one video frame.
// - Attempts to read one frame from |video_pool_|, and compresses it using
//   |video_encoder_| when a frame is available.
// - Passes the compressed frame to the video muxer for muxing.
int WebmEncoder::EncodeVideoFrame() {
  LiveWebmMuxer* video_muxer;
  if (config_.dash_encode) {
    video_muxer = ptr_muxer_vid_.get();
  } else {
    video_muxer = ptr_muxer_.get();
  }

  // Try reading a video frame from the pool.
  int status = video_pool_.Decommit(&raw_frame_);
  if (status) {
    if (status != BufferPool<VideoFrame>::kEmpty) {
      LOG(ERROR) << "VideoFrame pool Decommit failed! " << status;
      return kVideoSinkError;
    }
    VLOG(4) << "No frames in VideoFrame pool";
    return kSuccess;
  }

  VLOG(4) << "Encoder thread read raw frame.";

  status = OffsetTimestamp(timestamp_offset_, &raw_frame_);
  if (status) {
    LOG(ERROR) << "Video frame timestamp offset failed: " << status;
    return kVideoEncoderError;
  }

  // Encode the video frame, and pass it to the muxer.
  status = video_encoder_.EncodeFrame(raw_frame_, &vpx_frame_);
  if (status == kDropped) {
    return kSuccess;
  } else if (status) {
    LOG(ERROR) << "Video frame encode failed: " << status;
    return kVideoEncoderError;
  }

  // Update encoded duration if able to obtain the lock.
  std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
  if (lock.owns_lock()) {
    encoded_duration_ = std::max(vpx_frame_.timestamp(), encoded_duration_);
  }

  status = video_muxer->WriteVideoFrame(vpx_frame_);
  if (status) {
    LOG(ERROR) << "Video frame mux failed: " << status;
  }
  VLOG(3) << "muxed (V) " << vpx_frame_.timestamp() / 1000.0;
  return status;
}

int WebmEncoder::EncodeAudioBuffer() {
  // Try reading an audio buffer from the pool.
  int status = audio_pool_.Decommit(&raw_audio_buffer_);
  if (status) {
    if (status != BufferPool<AudioBuffer>::kEmpty) {
      // Really an error; not just an empty pool.
      LOG(ERROR) << "AudioBuffer pool Decommit failed! " << status;
      return kAudioSinkError;
    }
    VLOG(4) << "No buffers in AudioBuffer pool";
  } else {
    VLOG(4) << "Encoder thread read raw audio buffer.";

    status = OffsetTimestamp(timestamp_offset_, &raw_audio_buffer_);
    if (status) {
      LOG(ERROR) << "audio timestamp offset failed: " << status;
      return kAudioEncoderError;
    }

    // Pass the uncompressed audio to libvorbis.
    status = vorbis_encoder_.Encode(raw_audio_buffer_);
    if (status) {
      LOG(ERROR) << "vorbis encode failed " << status;
      return kAudioEncoderError;
    }
  }
  return kSuccess;
}

int WebmEncoder::WaitForSamples() {
  // Wait for samples from the input stream(s).
  bool got_audio = config_.disable_audio;
  bool got_video = config_.disable_video;
  for (;;) {
    if (StopRequested()) {
      return kSuccess;
    }
    if (!got_audio) {
      got_audio = !audio_pool_.IsEmpty();
    }
    if (!got_video) {
      got_video = !video_pool_.IsEmpty();
    }
    if (got_audio && got_video) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  int64 first_audio_timestamp = 0;
  if (!config_.disable_audio) {
    int64& a_ts = first_audio_timestamp;
    const int status = audio_pool_.ActiveBufferTimestamp(&a_ts);
    if (status) {
      LOG(ERROR) << "cannot read first audio timestamp: " << status;
      return status;
    }
  }

  int64 first_video_timestamp = 0;
  if (!config_.disable_video) {
    int64& v_ts = first_video_timestamp;
    const int status = video_pool_.ActiveBufferTimestamp(&v_ts);
    if (status) {
      LOG(ERROR) << "cannot read first video timestamp: " << status;
      return status;
    }
  }

  if (first_audio_timestamp < 0 && first_video_timestamp < 0) {
    const int first_a_ts = std::abs(static_cast<int>(first_audio_timestamp));
    const int first_v_ts = std::abs(static_cast<int>(first_video_timestamp));
    timestamp_offset_ = std::max(first_a_ts, first_v_ts);
  } else if (first_audio_timestamp < 0) {
    const int first_a_ts = std::abs(static_cast<int>(first_audio_timestamp));
    timestamp_offset_ = first_a_ts;
  } else if (first_video_timestamp < 0) {
    const int first_v_ts = std::abs(static_cast<int>(first_video_timestamp));
    timestamp_offset_ = first_v_ts;
  }
  LOG(INFO) << "WebmEncoder timestamp_offset_=" << timestamp_offset_;
  return kSuccess;
}

int WebmEncoder::PeekVideoTimestamp(int64* timestamp) {
  CHECK_NOTNULL(timestamp);
  const int status = video_pool_.ActiveBufferTimestamp(timestamp);
  if (status < 0) {
    LOG(ERROR) << "VideoFrame pool timestamp check failed: " << status;
    return kVideoSinkError;
  }
  if (status == BufferPool<VideoFrame>::kEmpty) {
    // When |video_pool_| is empty use the timestamp of the last encoded video
    // frame added to the calculated time per frame.
    const int time_per_frame =
        static_cast<int>(kTimebase / config_.actual_video_config.frame_rate);
    *timestamp = video_encoder_.last_timestamp() + time_per_frame;
    VLOG(3) << "NO video frame available ts=" << *timestamp;
  } else {
    *timestamp += timestamp_offset_;
    VLOG(3) << "video frame available ts=" << *timestamp;
  }
  return status;
}

int WebmEncoder::WriteMuxerChunkToDataSink(
    std::unique_ptr<LiveWebmMuxer>* muxer) {
  if (ptr_data_sink_->Ready()) {
    int32 chunk_length = 0;
    const bool chunk_ready = (*muxer)->ChunkReady(&chunk_length);
    if (chunk_ready) {
      const int64 chunk_num = (*muxer)->chunks_read();
      std::string id = NextChunkId((*muxer)->muxer_id(), chunk_num);
      // A complete chunk is waiting in |muxer|'s buffer.
      if (!ReadChunkFromMuxer(muxer, chunk_length)) {
        LOG(ERROR) << "cannot read WebM chunk from muxer_id: "
                   << (*muxer)->muxer_id();
        return kWebmMuxerError;
      }
#if 0
      // Pass the chunk to |ptr_data_sink_|.
      if (!ptr_data_sink_->WriteData(chunk_buffer_.get(), chunk_length, id)) {
        LOG(ERROR) << "data sink write failed!";
        return kDataSinkWriteFail;
      }
#endif
      // HACK: HERE BE DRAGONS
      CHECK(WriteChunkFile(config_.dash_dir + id,
                           chunk_buffer_.get(), chunk_length));
    }
  }
  return kSuccess;
}

int WebmEncoder::WriteLastMuxerChunkToDataSink(
    std::unique_ptr<LiveWebmMuxer>* muxer) {
  int status = (*muxer)->Finalize();
  if (status) {
    LOG(ERROR) << "muxer Finalize failed, muxer_id: " << (*muxer)->muxer_id()
               << " status: " << status;
    return status;
  }
  int32 chunk_length = 0;
  if ((*muxer)->ChunkReady(&chunk_length)) {
    LOG(INFO) << "mkvmuxer Finalize produced a chunk.";
    const int64 chunk_num = (*muxer)->chunks_read();
    std::string id = NextChunkId((*muxer)->muxer_id(), chunk_num);

    while (!ptr_data_sink_->Ready())
      std::this_thread::sleep_for(std::chrono::milliseconds(1));

    if (ReadChunkFromMuxer(muxer, chunk_length)) {
#if 0
      const bool sink_write_ok =
          ptr_data_sink_->WriteData(chunk_buffer_.get(), chunk_length, id);
      if (!sink_write_ok) {
        LOG(ERROR) << "data sink write fail on final chunk for muxer_id:"
                   << (*muxer)->muxer_id();
      } else {
        LOG(INFO) << "Final chunk upload initiated.";
      }
#endif
      // HACK: HERE BE DRAGONS
      CHECK(WriteChunkFile(config_.dash_dir + id,
                           chunk_buffer_.get(), chunk_length));
    }
  }
  return status;
}

std::string WebmEncoder::NextChunkId(const std::string& muxer_id,
                                     int64 chunk_num) const {
  std::string id;
  if (config_.dash_encode) {
    AdaptationSet::MediaType media_type = (muxer_id == kAudioId) ?
        AdaptationSet::kAudio : AdaptationSet::kVideo;
    id = dash_writer_->IdForChunk(media_type, chunk_num);
  } else {
    const char kHeader[] = "header";
    const char kChunk[] = "chunk";
    if (chunk_num == 0)
      id = kHeader;
    else
      id = kChunk;
  }
  LOG(INFO) << "chunk id: " << id;
  return id;
}

}  // namespace webmlive
