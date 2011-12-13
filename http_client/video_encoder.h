// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef HTTP_CLIENT_VIDEO_ENCODER_H_
#define HTTP_CLIENT_VIDEO_ENCODER_H_

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
#endif  // HTTP_CLIENT_VIDEO_ENCODER_H_
