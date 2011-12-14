// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef HTTP_CLIENT_VIDEO_ENCODER_H_
#define HTTP_CLIENT_VIDEO_ENCODER_H_

namespace webmlive {

// Temporary forward declaration of |VideoFrame| for
// |VideoFrameCallbackInterface|.
class VideoFrame;

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
  // Passes a |VideoFrame| pointer to the |VideoFrameCallbackInterface|
  // implementation.
  virtual int32 OnVideoFrameReceived(VideoFrame* ptr_frame) = 0;
};

struct VpxConfig {
  // Time between keyframes, in seconds.
  double keyframe_interval;
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

}  // namespace webmlive

#endif  // HTTP_CLIENT_VIDEO_ENCODER_H_
