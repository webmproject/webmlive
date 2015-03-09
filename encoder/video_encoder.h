// Copyright (c) 2012 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef WEBMLIVE_ENCODER_VIDEO_ENCODER_H_
#define WEBMLIVE_ENCODER_VIDEO_ENCODER_H_

#include <memory>
#include <mutex>
#include <queue>

#include "encoder/basictypes.h"
#include "encoder/encoder_base.h"

namespace webmlive {

enum VideoFormat {
  kVideoFormatI420 = 0,
  kVideoFormatVP8 = 1,
  kVideoFormatYV12 = 2,
  kVideoFormatYUY2 = 3,
  kVideoFormatYUYV = 4,
  kVideoFormatUYVY = 5,
  kVideoFormatRGB = 6,
  kVideoFormatRGBA = 7,
  kVideoFormatVP9 = 8,
  kVideoFormatCount = 9,
};

// YUV bit count constants.
const uint16 kI420BitCount = 12;
const uint16 kNV12BitCount = 12;
const uint16 kNV21BitCount = 12;
const uint16 kUYVYBitCount = 16;
const uint16 kV210BitCount = 24;
const uint16 kYUY2BitCount = 16;
const uint16 kYUYVBitCount = 16;
const uint16 kYV12BitCount = 12;
const uint16 kYV16BitCount = 16;

// RGB bit count constants.
const uint16 kRGB555BitCount = 16;
const uint16 kRGB565BitCount = 16;
const uint16 kRGBBitCount = 24;
const uint16 kRGBABitCount = 32;

// Utility function for conversion of four character codes to members of the
// |VideoFormat| enumeration. Returns true and writes the |VideoFormat| value
// to |ptr_format| when |fourcc| is recognized. Returns false when |fourcc| is
// not recognized. Always returns false when |ptr_format| is NULL.
bool FourCCToVideoFormat(uint32 fourcc,
                         uint16 bits_per_pixel,
                         VideoFormat* ptr_format);

// Video configuration control structure. Values set to 0 mean use default.
// Only |width|, |height|, and |frame_rate| are configurable. |format| and
// |stride| are controlled by the input device.
// TODO(tomfinegan): Write a VideoConfig validator.
struct VideoConfig {
  VideoConfig()
      : format(kVideoFormatI420),
        width(0),
        height(0),
        stride(0),
        frame_rate(0) {}

  VideoFormat format;   // Video pixel format.
  int32 width;          // Width in pixels.
  int32 height;         // Height in pixels.
  int32 stride;
  double frame_rate;    // Frame rate in frames per second.
};

// Storage class for I420, YV12, and VPx video frames. The main idea here is to
// store frames in such a way that they can easily be obtained from the capture
// source and passed to the libvpx VPx encoder.
//
// Notes
// - Libvpx's VP8 encoder supports only I420 and YV12 input.
//   |VideoFrame::Init()| converts all uncompressed formats other than
//   |kVideoFormatI420| and |kVideoFormatYV12| to |kVideoFormatI420|.
// - Libvpx's VP9 encoder supports formats beyond those above, but support for
//   those formats is not implemented here.
class VideoFrame {
 public:
  enum {
    kConversionFailed = -3,
    kNoMemory = -2,
    kInvalidArg = -1,
    kSuccess = 0,
  };
  VideoFrame();
  ~VideoFrame();

  // Allocates storage for |ptr_data|, sets internal fields to values of
  // caller's args, and returns |kSuccess|. Returns |kInvalidArg| when
  // |ptr_data| is NULL. Returns |kConversionFailed| when |format| is not a
  // |VideoFormat| enumeration value. Returns |kNoMemory| when unable to
  // allocate storage for |ptr_data|.
  // Note: When format is not one of |kVideoFormatI420|, |kVideoFormatYV12|,
  //       |kVideoFormatVP8| or |kVideoFormatVP9|, |Init()| converts the frame
  //       data to I420.
  int Init(const VideoConfig& config,
           bool keyframe,
           int64 timestamp,
           int64 duration,
           const uint8* ptr_data,
           int32 data_length);

  // Copies |VideoFrame| data to |ptr_frame|. Performs allocation if necessary.
  // Returns |kSuccess| when successful. Returns |kInvalidArg| when |ptr_frame|
  // is NULL. Returns |kNoMemory| when memory allocation fails.
  int Clone(VideoFrame* ptr_frame) const;

  // Swaps |VideoFrame| member data with |ptr_frame|'s. The |VideoFrame|s
  // must have non-NULL buffers.
  void Swap(VideoFrame* ptr_frame);

  // Accessors/Mutators.
  bool keyframe() const { return keyframe_; }
  int32 width() const { return config_.width; }
  int32 height() const { return config_.height; }
  int32 stride() const { return config_.stride; }
  int64 timestamp() const { return timestamp_; }
  void set_timestamp(int64 timestamp) { timestamp_ = timestamp; }
  int64 duration() const { return duration_; }
  uint8* buffer() const { return buffer_.get(); }
  int32 buffer_length() const { return buffer_length_; }
  int32 buffer_capacity() const { return buffer_capacity_; }
  VideoFormat format() const { return config_.format; }
  const VideoConfig& config() const { return config_; }

 private:
  // Converts video frame from |config.format| to I420, and stores the I420
  // frame in |buffer_|. Returns |kSuccess| when successful. Returns
  // |kNoMemory| if unable to allocate storage for the converted video frame.
  // Note: Output stride is equal to |config.width| after conversion, and stored
  //       in |config_.stride|.
  int ConvertToI420(const VideoConfig& config, const uint8* ptr_data);

  bool keyframe_;
  int64 timestamp_;
  int64 duration_;
  std::unique_ptr<uint8[]> buffer_;
  int32 buffer_capacity_;
  int32 buffer_length_;
  VideoConfig config_;
  WEBMLIVE_DISALLOW_COPY_AND_ASSIGN(VideoFrame);
};

// Pure interface class that provides a simple callback allowing the
// implementor class to receive |VideoFrame| pointers.
class VideoFrameCallbackInterface {
 public:
  enum {
    // Returned by |OnVideoFrameReceived| when |ptr_frame| is NULL or empty.
    kInvalidArg = -2,
    kSuccess = 0,
    // Returned by |OnVideoFrameReceived| when |ptr_frame| is dropped.
    kDropped = 1,
  };
  virtual ~VideoFrameCallbackInterface() {}

  // Passes a |VideoFrame| pointer to the |VideoFrameCallbackInterface|
  // implementation, allowing it to take ownership of the contents. Argument
  // is non-const to allow for use of |VideoFrame::Swap| by the implementor.
  virtual int OnVideoFrameReceived(VideoFrame* ptr_frame) = 0;
};

struct VpxConfig {
  // Special value that means use the default value for the current option.
  static const int kUseDefault = -200;
  VpxConfig()
      : keyframe_interval(1000),
        bitrate(500),
        codec(kVideoFormatVP8),
        decimate(kUseDefault),
        min_quantizer(2),
        max_quantizer(52),
        speed(-6),
        static_threshold(kUseDefault),
        thread_count(kUseDefault),
        token_partitions(kUseDefault),
        undershoot(kUseDefault),
        noise_sensitivity(kUseDefault),
        overshoot(kUseDefault),
        total_buffer_time(1000),
        initial_buffer_time(500),
        optimal_buffer_time(600),
        max_keyframe_bitrate(300),
        sharpness(0),
        error_resilient(false),
        goldenframe_cbr_boost(300),
        adaptive_quantization_mode(3),
        tile_columns(4),
        disable_fpd(false) {}

  // Time between keyframes, in milliseconds.
  int keyframe_interval;

  // Video bitrate, in kilobits.
  int bitrate;

  // Video codec, kVideoFormatVP8 or kVideoFormatVP9.
  VideoFormat codec;

  // Video frame rate decimation factor.
  int decimate;

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

  // Reduces the noise level of uncompressed video before processing by
  // blurring the pixels of adjacent frames together.
  int noise_sensitivity;

  // Percentage to overshoot the requested datarate.
  int overshoot;

  // Client buffer sizes (values in milliseconds).
  int total_buffer_time;
  int initial_buffer_time;
  int optimal_buffer_time;

  // Maximum keyframe (I-frame) bitrate (percentage of |bitrate|).
  int max_keyframe_bitrate;

  // Loop filter sharpness, 0-7.
  int sharpness;

  // Error resilience on/off.
  bool error_resilient;

  // Golden frame bitrate boost in CBR (percentage of |bitrate|).
  int goldenframe_cbr_boost;

  // Adaptive quantization mode
  // 0: off
  // 1: variance
  // 2: complexity
  // 3: cyclic refresh (default)
  int adaptive_quantization_mode;

  // Number of tile columns, log2.
  int tile_columns;

  // Disables frame parallel decoding features.
  bool disable_fpd;
};

// Forward declaration of |VpxEncoder| class for use in |VideoEncoder|. The
// libvpx implementation details are kept hidden because use of the includes
// produces C4505 warnings with MSVC at warning level 4.
class VpxEncoder;
struct WebmEncoderConfig;

class VideoEncoder {
 public:
  enum {
    // Libvpx reported an error.
    kCodecError = -101,
    // Encoder error not originating from libvpx.
    kEncoderError = -100,
    kNoMemory = -2,
    kInvalidArg = -1,
    kSuccess = 0,
    kDropped = 1,
  };
  VideoEncoder();
  ~VideoEncoder();
  int32 Init(const WebmEncoderConfig& config);
  int32 EncodeFrame(const VideoFrame& raw_frame, VideoFrame* ptr_vpx_frame);

  // Accessors.
  int64 frames_in() const;
  int64 frames_out() const;
  int64 last_keyframe_time() const;
  int64 last_timestamp() const;

 private:
  std::unique_ptr<VpxEncoder> ptr_vpx_encoder_;
  WEBMLIVE_DISALLOW_COPY_AND_ASSIGN(VideoEncoder);
};

}  // namespace webmlive

#endif  // WEBMLIVE_ENCODER_VIDEO_ENCODER_H_

