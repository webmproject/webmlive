// Copyright (c) 2012 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef WEBMLIVE_ENCODER_WIN_DSHOW_UTIL_H_
#define WEBMLIVE_ENCODER_WIN_DSHOW_UTIL_H_

#include <ios>

#include "webmdshow/common/odbgstream.h"  // NOLINT
// Above include NOLINT'd because it *must always* come before hrtext.
#include "webmdshow/common/hrtext.h"

// Extracts error from the HRESULT, and outputs its hex and decimal values.
#define HRLOG(X) \
    " {" << #X << "=" << X << "/" << std::hex << X << std::dec << " (" << \
    hrtext(X) << ")}"

#endif  // WEBMLIVE_ENCODER_WIN_DSHOW_UTIL_H_
