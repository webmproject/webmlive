// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef HTTP_CLIENT_WEBM_ENCODER_H_
#define HTTP_CLIENT_WEBM_ENCODER_H_


// <string> includes <cstdlib> (indirectly), which leads to deprecation
// warnings, disable them.
#pragma warning(disable:4995)
#include <string>
#pragma warning(default:4995)

#include "boost/scoped_ptr.hpp"
#include "http_client/basictypes.h"
#include "http_client/http_client_base.h"

namespace webmlive {
// Special value interpreted by |WebmEncoder| as "use implementation default".
const int kUseEncoderDefault = -200;
// Special value interpreted by |WebmEncoder| as "use capture device default".
const int kUseDeviceDefault = -200;

// Video capture defaults.
const int kDefaultVideoWidth = kUseDeviceDefault;
const int kDefaultVideoHeight = kUseDeviceDefault;
const int kDefaultVideoFrameRate = kUseDeviceDefault;
// Vorbis defaults.
const int kDefaultVorbisBitrate = kUseEncoderDefault;
// VP8 defaults.
const double kDefaultVpxKeyframeInterval = 1.0;
const int kDefaultVpxBitrate = 500;
const int kDefaultVpxMinQ = 10;
const int kDefaultVpxMaxQ = 46;
const int kDefaultVpxSpeed = kUseEncoderDefault;
const int kDefaultVpxStaticThreshold = kUseEncoderDefault;
const int kDefaultVpxUndershoot = kUseEncoderDefault;
const int kDefaultVpxThreadCount = kUseEncoderDefault;
const int kDefaultVpxTokenPartitions = kUseEncoderDefault;

struct WebmEncoderConfig {
  struct VideoCaptureConfig {
    // Width, in pixels.
    int width;
    // Height, in pixels.
    int height;
    // Frame rate, in frames per second.
    double frame_rate;
  };
  struct VpxConfig {
    // Time between keyframes, in seconds.
    double keyframe_interval;
    // Video bitrate, in kilobits.
    int bitrate;
    // Minimum quantizer value.
    int min_quantizer;
    // Maxium quantizer value.
    int max_quantizer;
    // Encoder complexity.
    int speed;
    // Threshold at which a macroblock is considered static.
    int static_threshold;
    // Encoder thead count.
    int thread_count;
    // Number of token partitions.
    int token_partitions;
    // Percentage to undershoot the requested datarate.
    int undershoot;
  };
  // Vorbis encoder bitrate.
  int vorbis_bitrate;
  // Output file name.
  std::string output_file_name;
  // Name of the audio device.  Leave empty to use system default.
  std::string audio_device_name;
  // Name of the video device.  Leave empty to use system default.
  std::string video_device_name;
  // Video capture settings.
  VideoCaptureConfig video_config;
  // VP8 encoder settings.
  VpxConfig vpx_config;
};

class WebmEncoderImpl;

// Basic encoder interface class intended to hide platform specific encoder
// implementation details.
class WebmEncoder {
 public:
  enum {
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
    kSuccess = 0,
  };
  WebmEncoder();
  ~WebmEncoder();
  // Initializes the encoder. Returns |kSuccess| upon success, or one of the
  // above status codes upon failure.
  int Init(const WebmEncoderConfig& config);
  // Runs the encoder. Returns |kSuccess| when successful, or one of the above
  // status codes upon failure.
  int Run();
  // Stops the encoder.
  void Stop();
  // Returns encoded duration in seconds.
  double encoded_duration();
  // Returns |WebmEncoderConfig| with fields set to default values.
  static WebmEncoderConfig DefaultConfig();
 private:
  // Encoder object.
  boost::scoped_ptr<WebmEncoderImpl> ptr_encoder_;
  WEBMLIVE_DISALLOW_COPY_AND_ASSIGN(WebmEncoder);
};

}  // namespace webmlive

#endif  // HTTP_CLIENT_WEBM_ENCODER_H_
