// Copyright (c) 2012 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef CLIENT_ENCODER_WEBM_ENCODER_H_
#define CLIENT_ENCODER_WEBM_ENCODER_H_

#include <string>

#include "boost/scoped_array.hpp"
#include "boost/scoped_ptr.hpp"
#include "boost/shared_ptr.hpp"
#include "boost/thread/thread.hpp"
#include "client_encoder/basictypes.h"
#include "client_encoder/data_sink.h"
#include "client_encoder/client_encoder_base.h"
#include "client_encoder/video_encoder.h"

namespace webmlive {
// All timestamps are in milliseconds.
const int kTimebase = 1000;

struct WebmEncoderConfig {
  // Audio configuration control structure. Values set to 0 mean use default.
  // Only |channels|, |sample_rate|, and |bits_per_sample| are user
  // configurable.
  struct AudioConfig {
    const static uint16 kFormatPcm = 1;
    const static uint16 kFormatIeeeFloat = 3;
    AudioConfig()
        : format_tag(kFormatPcm),
          channels(2),
          bytes_per_second(0),
          sample_rate(44100),
          block_align(0),
          bits_per_sample(16),
          valid_bits_per_sample(0),
          channel_mask(0) {}

    uint16 format_tag;              // Audio format.
    uint16 channels;                // Number of channels.
    uint32 sample_rate;             // Samples per second.
    uint32 bytes_per_second;        // Average bytes per second.
    uint16 block_align;             // Atomic audio unit size in bytes.
    uint16 bits_per_sample;         // Sample container size.
    uint16 valid_bits_per_sample;   // Valid bits in sample container.
    uint32 channel_mask;            // Channels present in audio stream.
  };

  // User interface control structure. |MediaSourceImpl| will attempt to
  // display configuration control dialogs when fields are set to true.
  struct UserInterfaceOptions {
    UserInterfaceOptions()
        : manual_audio_config(false),
          manual_video_config(false) {}

    bool manual_audio_config;   // Show audio source configuration interface.
    bool manual_video_config;   // Show video source configuration interface.
  };

  WebmEncoderConfig() : vorbis_bitrate(128) {}

  // Vorbis encoder bitrate.
  int32 vorbis_bitrate;

  // Name of the audio device.  Leave empty to use system default.
  std::string audio_device_name;

  // Name of the video device.  Leave empty to use system default.
  std::string video_device_name;

  // Requested audio capture settings.
  AudioConfig requested_audio_config;

  // Actual audio capture settings.
  AudioConfig actual_audio_config;

  // Requested video capture settings.
  VideoConfig requested_video_config;

  // Actual video capture settings.
  VideoConfig actual_video_config;

  // VP8 encoder settings.
  VpxConfig vpx_config;

  // Source device options.
  UserInterfaceOptions ui_opts;
};

class MediaSourceImpl;
class LiveWebmMuxer;

// Top level WebM encoder class. Manages capture from A/V input devices, VP8
// encoding, Vorbis encoding, and muxing into a WebM stream.
class WebmEncoder : public VideoFrameCallbackInterface {
 public:
  // Default size of |chunk_buffer_|.
  static const int kDefaultChunkBufferSize = 100 * 1024;
  enum {
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

  // VideoFrameCallbackInterface methods
  // Method used by MediaSourceImpl to push video frames into |EncoderThread|.
  virtual int32 OnVideoFrameReceived(VideoFrame* ptr_frame);

 private:
  // Returns true when user wants the encode thread to stop.
  bool StopRequested();

  // Reads chunk from |ptr_muxer_| and reallocates |chunk_buffer_| when
  // necessary. Returns true when successful.
  bool ReadChunkFromMuxer(int32 chunk_length);

  // Encoding thread function.
  void EncoderThread();

  // Set to true when |Init()| is successful.
  bool initialized_;

  // Flag protected by |mutex_| and used by |EncoderThread| via |StopRequested|
  // to determine when to terminate.
  bool stop_;

  // Temporary storage for chunks about to be passed to |ptr_data_sink_|.
  boost::scoped_array<uint8> chunk_buffer_;
  int32 chunk_buffer_size_;

  // Pointer to platform specific audio/video source object implementation.
  boost::scoped_ptr<MediaSourceImpl> ptr_media_source_;

  // Pointer to live WebM muxer.
  boost::scoped_ptr<LiveWebmMuxer> ptr_muxer_;

  // Mutex providing synchronization between user interface and encoder thread.
  mutable boost::mutex mutex_;

  // Encoder thread object.
  boost::shared_ptr<boost::thread> encode_thread_;

  // Data sink to which WebM chunks are written.
  DataSinkInterface* ptr_data_sink_;

  // Queue used to push video frames from |MediaSourceImpl| into
  // |EncoderThread|.
  VideoFrameQueue video_queue_;

  // Most recent frame from |video_queue_|.
  VideoFrame raw_frame_;

  // Most recent frame from |video_encoder_|.
  VideoFrame vp8_frame_;

  // Video encoder.
  VideoEncoder video_encoder_;

  // Encoded duration in milliseconds.
  int64 encoded_duration_;

  // Encoder configuration.
  WebmEncoderConfig config_;
  WEBMLIVE_DISALLOW_COPY_AND_ASSIGN(WebmEncoder);
};

}  // namespace webmlive

#endif  // CLIENT_ENCODER_WEBM_ENCODER_H_
