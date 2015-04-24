// Copyright (c) 2015 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef WEBMLIVE_ENCODER_WIN_CAPTURE_SOURCE_LIST_DSHOW_H_
#define WEBMLIVE_ENCODER_WIN_CAPTURE_SOURCE_LIST_DSHOW_H_

#include <string>

namespace webmlive {

// Returns list of audio capture in the format: index: name, or an empty string
// upon failure.
std::string GetAudioSourceList();

// Returns list of video capture in the format: index: name, or an empty string
// upon failure.
std::string GetVideoSourceList();

}  // namespace webmlive

#endif  // WEBMLIVE_ENCODER_WIN_CAPTURE_SOURCE_LIST_DSHOW_H_
