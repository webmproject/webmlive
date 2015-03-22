// Copyright (c) 2012 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef WEBMLIVE_ENCODER_WEBM_ENCODER_H_
#define WEBMLIVE_ENCODER_WEBM_ENCODER_H_

#include <memory>
#include <string>
#include <thread>

#include "encoder/audio_encoder.h"
#include "encoder/basictypes.h"
#include "encoder/buffer_pool.h"
#include "encoder/encoder_base.h"
#include "encoder/data_sink.h"
#include "encoder/video_encoder.h"
#include "encoder/vorbis_encoder.h"

namespace webmlive {
// All timestamps are in milliseconds.
const int kTimebase = 1000;
// Special value meaning use system default device.
const int kUseDefaultDevice = -1;

struct WebmEncoderConfig {
  // User interface control structure. |MediaSourceImpl| will attempt to
  // display configuration control dialogs when fields are set to true.
  struct UserInterfaceOptions {
    UserInterfaceOptions()
        : manual_audio_config(false),
          manual_video_config(false) {}

    bool manual_audio_config;   // Show audio source configuration interface.
    bool manual_video_config;   // Show video source configuration interface.
  };

  WebmEncoderConfig()
      : disable_audio(false),
        disable_video(false),
        audio_device_index(kUseDefaultDevice),
        video_device_index(kUseDefaultDevice),
        dash_encode(false),
        dash_name("webmlive"),
        dash_dir("./"),
        dash_start_number("1") {}

  // Audio/Video disable flags.
  bool disable_audio;
  bool disable_video;

  // Name of the audio device.  Leave empty to use system default.
  std::string audio_device_name;

  // Audio device index. Leave set to |kUseDefaultDevice| to use system default.
  int audio_device_index;

  // Name of the video device.  Leave empty to use system default.
  std::string video_device_name;

  // Video device index. Leave set to |kUseDefaultDevice| to use system default.
  int video_device_index;

  // Requested audio capture settings.
  AudioConfig requested_audio_config;

  // Actual audio capture settings.
  AudioConfig actual_audio_config;

  // Requested video capture settings.
  VideoConfig requested_video_config;

  // Actual video capture settings.
  VideoConfig actual_video_config;

  // Vorbis audio encoder settings.
  VorbisConfig vorbis_config;

  // VPx encoder settings.
  VpxConfig vpx_config;

  // Source device options.
  UserInterfaceOptions ui_opts;

  // Enable DASH encoding mode.
  bool dash_encode;

  // MPD name and DASH chunk ID prefix.
  std::string dash_name;

  // Output directory for MPD and DASH chunks.
  std::string dash_dir;

  // MPD SegmentTemplate startNumber value.
  std::string dash_start_number;
};

class DashWriter;
class MediaSourceImpl;
class LiveWebmMuxer;

// Top level WebM encoder class. Manages capture from A/V input devices, VPx
// encoding, Vorbis encoding, and muxing into a WebM stream.
class WebmEncoder : public AudioSamplesCallbackInterface,
                    public VideoFrameCallbackInterface {
 public:
  // Default size of |chunk_buffer_|.
  static const int kDefaultChunkBufferSize = 100 * 1024;
  enum {
    // Data sink write failed.
    kDataSinkWriteFail = -117,

    // AV capture implementation unable to setup audio buffer sink.
    kAudioSinkError = -116,

    // AV capture source stopped on its own.
    kAVCaptureStopped = -115,

    // AV capture implementation unable to setup video frame sink.
    kVideoSinkError = -114,

    // Encoder implementation unable to configure audio source.
    kAudioConfigureError = -113,

    // Encoder implementation unable to configure video source.
    kVideoConfigureError = -112,

    // Encoder implementation unable to monitor encoder state.
    kEncodeMonitorError = -111,

    // Encoder implementation unable to control encoder.
    kEncodeControlError = -110,

    // Encoder implementation file writing related error.
    kFileWriteError = -109,

    // Encoder implementation WebM muxing related error.
    kWebmMuxerError = -108,

    // Encoder implementation audio encoding related error.
    kAudioEncoderError = -107,

    // Encoder implementation video encoding related error.
    kVideoEncoderError = -106,

    // Invalid argument passed to method.
    kInvalidArg = -105,

    // Operation not implemented.
    kNotImplemented = -104,

    // Unable to find an audio source.
    kNoAudioSource = -103,

    // Unable to find a video source.
    kNoVideoSource = -102,

    // Encoder implementation initialization failed.
    kInitFailed = -101,

    // Cannot run the encoder.
    kRunFailed = -100,
    kNoMemory = -2,
    kInvaligArg = -1,
    kSuccess = 0,
  };

  WebmEncoder();
  ~WebmEncoder();

  // Initializes the encoder. Returns |kSuccess| upon success, or one of the
  // above status codes upon failure. Always returns |kInvalidArg| when
  // |ptr_data_sink| is NULL.
  int Init(const WebmEncoderConfig& config, DataSinkInterface* ptr_data_sink);

  // Runs the encoder. Returns |kSuccess| when successful, or one of the above
  // status codes upon failure.
  int Run();

  // Stops the encoder.
  void Stop();

  // Returns encoded duration in milliseconds.
  int64 encoded_duration() const;

  // Returns |WebmEncoderConfig| with fields set to default values.
  static WebmEncoderConfig DefaultConfig();
  WebmEncoderConfig config() const { return config_; }

  // |AudioSamplesCallbackInterface| methods
  // Method used by |MediaSourceImpl| to push audio buffers into
  // |EncoderThread()|.
  virtual int OnSamplesReceived(AudioBuffer* ptr_buffer);

  // |VideoFrameCallbackInterface| methods
  // Method used by |MediaSourceImpl| to push video frames into
  // |EncoderThread()|.
  virtual int OnVideoFrameReceived(VideoFrame* ptr_frame);

 private:
  // Function pointer type used for indirect access to the encoder loop
  // methods from |EncoderThread()|.
  typedef int (WebmEncoder::*EncoderLoopFunc)();

  // Returns true when user wants the encode thread to stop.
  bool StopRequested();

  // Reads chunk from |muxer| and reallocates |chunk_buffer_| when necessary.
  // Returns true when successful.
  bool ReadChunkFromMuxer(std::unique_ptr<LiveWebmMuxer>* muxer,
                          int32 chunk_length);

  // Encoding thread function.
  void EncoderThread();

  // Audio/Video |EncoderLoopFunc|s. Called by |EncoderThread()| via
  // |ptr_encode_func_|. All loop functions return |kSuccess| when the encode
  // pass succeeds.
  int EncodeAudioOnly();
  int AVEncode();
  int EncodeVideoFrame();
  int DashEncode();

  // Utility function used to encode a single audio input buffer.
  int EncodeAudioBuffer();

  // Waits for input samples from |ptr_media_source_| and sets
  // |timestamp_offset_| when one or both streams start with a negative
  // timestamp.
  int WaitForSamples();

  // Returns the timestamp of the next available video frame via |timestamp|.
  int PeekVideoTimestamp(int64* timestamp);

  // Writes |muxer| chunk to |ptr_data_sink_| when |muxer->ChunkReady()|
  // returns true.
  int WriteMuxerChunkToDataSink(std::unique_ptr<LiveWebmMuxer>* muxer);

  // Writes last chunk from |muxer| to |ptr_data_sink_| and finalizes |muxer|.
  int WriteLastMuxerChunkToDataSink(std::unique_ptr<LiveWebmMuxer>* muxer);

  // Returns a chunk identifier for |chunk_num| from |muxer|.
  std::string NextChunkId(const std::string& muxer_id,
                          int64 chunk_num) const;

  // Set to true when |Init()| is successful.
  bool initialized_;

  // Flag protected by |mutex_| and used by |EncoderThread()| via
  // |StopRequested()| to determine when to terminate.
  bool stop_;

  // Temporary storage for chunks about to be passed to |ptr_data_sink_|.
  std::unique_ptr<uint8[]> chunk_buffer_;
  int32 chunk_buffer_size_;

  // Pointer to platform specific audio/video source object implementation.
  std::unique_ptr<MediaSourceImpl> ptr_media_source_;

  // Pointer to live WebM muxer. |ptr_muxer_| is used for muxed A/V output and
  // single stream output.
  std::unique_ptr<LiveWebmMuxer> ptr_muxer_;

  // Pointers to live WebM muxers. |ptr_muxer_aud_| and |ptr_muxer_vid_| are
  // used for DASH encodes that do not mux audio and video into the same WebM
  // chunks.
  // TODO(tomfinegan): Support multiple streams of each media type.
  std::unique_ptr<LiveWebmMuxer> ptr_muxer_aud_;
  std::unique_ptr<LiveWebmMuxer> ptr_muxer_vid_;

  // Mutex providing synchronization between user interface and encoder thread.
  mutable std::mutex mutex_;

  // Encoder thread object.
  std::shared_ptr<std::thread> encode_thread_;

  // Data sink to which WebM chunks are written.
  DataSinkInterface* ptr_data_sink_;

  // Buffer object used to push |VideoFrame|s from |MediaSourceImpl| into
  // |EncoderThread()|.
  BufferPool<VideoFrame> video_pool_;

  // Most recent frame from |video_pool_|.
  VideoFrame raw_frame_;

  // Most recent frame from |video_encoder_|.
  VideoFrame vpx_frame_;

  // Video encoder.
  VideoEncoder video_encoder_;

  // Encoded duration in milliseconds.
  int64 encoded_duration_;

  // Buffer object used to push |AudioBuffer|s from |MediaSourceImpl| into
  // |EncoderThread()|.
  BufferPool<AudioBuffer> audio_pool_;

  // Most recent uncompressed audio buffer from |audio_pool_|.
  AudioBuffer raw_audio_buffer_;

  // Most recent vorbis audio buffer from |vorbis_encoder_|.
  AudioBuffer vorbis_audio_buffer_;

  // Vorbis encoder object.
  VorbisEncoder vorbis_encoder_;

  // Encoder configuration.
  WebmEncoderConfig config_;

  // Encoder loop function pointer.
  EncoderLoopFunc ptr_encode_func_;

  // DASH manifest writer.
  std::unique_ptr<DashWriter> dash_writer_;

  // Timestamp adjustment value. Expressed in milliseconds. Used to change
  // input buffer timestamps when a stream starts with a timestamp less than 0.
  int64 timestamp_offset_;
  WEBMLIVE_DISALLOW_COPY_AND_ASSIGN(WebmEncoder);
};

}  // namespace webmlive

#endif  // WEBMLIVE_ENCODER_WEBM_ENCODER_H_

