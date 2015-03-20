// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef WEBMLIVE_ENCODER_ENCODER_BASE_H_
#define WEBMLIVE_ENCODER_ENCODER_BASE_H_

#if _WIN32

#ifndef _WIN32_WINNT
#  define _WIN32_WINNT 0x0501  // WinXP
#endif

// Disable the max macro defined in the windows headers.
#define NOMINMAX

#include <windows.h>
#ifdef ERROR
#  undef ERROR  // unused by webmlive/collides with glog.
#endif

#endif  // _WIN32

// App Version/Identity
namespace webmlive {

static const char* kEncoderName = "webmlive encoder";
static const char* kEncoderVersion = "2.0.0.0";

}  // namespace webmlive

#endif  // WEBMLIVE_ENCODER_ENCODER_BASE_H_
