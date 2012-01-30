// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef HTTP_CLIENT_VIDEO_ENCODER_H_
#define HTTP_CLIENT_VIDEO_ENCODER_H_

#include <queue>

#include "boost/scoped_array.hpp"
#include "boost/scoped_ptr.hpp"
#include "boost/thread/mutex.hpp"
#include "http_client/basictypes.h"
#include "http_client/http_client_base.h"

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
  kVideoFormatCount = 8,
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

// Storage class for I420, YV12, and VP8 video frames. The main idea here is to
// store frames in such a way that they can easily be obtained from the capture
// source and passed to the libvpx VP8 encoder.
//
// Notes
// - Libvpx's VP8 encoder supports only I420 and YV12 input.
//   |VideoFrame::Init()| converts all uncompressed formats other than
//   |kVideoFormatI420| and |kVideoFormatYV12| to |kVideoFormatI420|.
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
  //       |kVideoFormatVP8|, |Init()| converts the frame data to I420.
  int32 Init(VideoFormat format,
             bool keyframe,
             int32 width,
             int32 height,
             int32 stride,
             int64 timestamp,
             int64 duration,
             const uint8* ptr_data,
             int32 data_length);

  // Copies |VideoFrame| data to |ptr_frame|. Performs allocation if necessary.
  // Returns |kSuccess| when successful. Returns |kInvalidArg| when |ptr_frame|
  // is NULL. Returns |kNoMemory| when memory allocation fails.
  int32 Clone(VideoFrame* ptr_frame) const;

  // Swaps |VideoFrame| member data with |ptr_frame|'s. The |VideoFrame|s
  // must have non-NULL buffers.
  void Swap(VideoFrame* ptr_frame);

  // Accessors.
  bool keyframe() const { return keyframe_; }
  int32 width() const { return width_; }
  int32 height() const { return height_; }
  int32 stride() const { return stride_; }
  int64 timestamp() const { return timestamp_; }
  int64 duration() const { return duration_; }
  uint8* buffer() const { return buffer_.get(); }
  int32 buffer_length() const { return buffer_length_; }
  int32 buffer_capacity() const { return buffer_capacity_; }
  VideoFormat format() const { return format_; }

 private:
  // Converts video frame from |format| to I420, and stores the I420 frame in
  // |buffer_|. Returns |kSuccess| when successful. Returns |kNoMemory| if
  // unable to allocate storage for the converted video frame.
  // Note: Output strude is equal to |width| after conversion, and stored in
   //      |stride_|.
  int32 ConvertToI420(VideoFormat format,
                      int32 width,
                      int32 height,
                      int32 stride,
                      const uint8* ptr_data);

  bool keyframe_;
  int32 width_;
  int32 height_;
  int32 stride_;
  int64 timestamp_;
  int64 duration_;
  boost::scoped_array<uint8> buffer_;
  int32 buffer_capacity_;
  int32 buffer_length_;
  VideoFormat format_;
  WEBMLIVE_DISALLOW_COPY_AND_ASSIGN(VideoFrame);
};

// Queue object used to pass video frames between threads. Uses two
// |std::queue<VideoFrame*>|s and moves |VideoFrame| pointers between them to
// provide a means by which the capture thread can pass samples to the video
// encoder.
class VideoFrameQueue {
 public:
  enum {
    // |Push| called before |Init|.
    kNoBuffers = -5,
    // No |VideoFrame|s waiting in |active_frames_|.
    kEmpty = -4,
    // No |VideoFrame|s available in |frame_pool_|.
    kFull = -3,
    kNoMemory = -2,
    kInvalidArg = -1,
    kSuccess = 0,
  };

  // Number of |VideoFrame|'s to allocate and push into the |frame_pool_|.
  static const int32 kQueueLength = 4;
  VideoFrameQueue();
  ~VideoFrameQueue();

  // Allocates |kQueueLength| |VideoFrame|s, pushes them into |frame_pool_|, and
  // returns |kSuccess|.
  int32 Init();

  // Grabs a |VideoFrame| from |frame_pool_|, copies the data from |ptr_frame|,
  // and pushes it into |active_frames_|. Returns |kSuccess| if able to store
  // the frame. Returns |kFull| when |frame_pool_| is empty. Avoids copy using
  // |VideoFrame::Swap| whenever possible.
  int32 Commit(VideoFrame* ptr_frame);

  // Grabs a |VideoFrame| from |active_frames_| and copies it to |ptr_frame|.
  // Returns |kSuccess| when able to copy the frame. Returns |kEmpty| when
  // |active_frames_| contains no |VideoFrame|s.
  int32 Read(VideoFrame* ptr_frame);

  // Drops all queued |VideoFrame|s by moving them all from |active_frames_| to
  // |frame_pool_|.
  void DropFrames();

 private:
  // Moves or copies |ptr_source| to |ptr_target| using |VideoFrame::Swap| or
  // |VideoFrame::Clone| based on presence of non-NULL buffer pointer in
  // |ptr_target|.
  int32 ExchangeFrames(VideoFrame* ptr_source, VideoFrame* ptr_target);

  boost::mutex mutex_;
  std::queue<VideoFrame*> frame_pool_;
  std::queue<VideoFrame*> active_frames_;
  WEBMLIVE_DISALLOW_COPY_AND_ASSIGN(VideoFrameQueue);
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
  virtual ~VideoFrameCallbackInterface();
  // Passes a |VideoFrame| pointer to the |VideoFrameCallbackInterface|
  // implementation.
  virtual int32 OnVideoFrameReceived(VideoFrame* ptr_frame) = 0;
};

struct VpxConfig {
  // Time between keyframes, in milliseconds.
  int keyframe_interval;
  // Video bitrate, in kilobits.
  int bitrate;
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
  int32 EncodeFrame(const VideoFrame& raw_frame, VideoFrame* ptr_vp8_frame);
 private:
  boost::scoped_ptr<VpxEncoder> ptr_vpx_encoder_;
  WEBMLIVE_DISALLOW_COPY_AND_ASSIGN(VideoEncoder);
};

}  // namespace webmlive

#endif  // HTTP_CLIENT_VIDEO_ENCODER_H_

