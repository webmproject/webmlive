// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef WEBMLIVE_FILE_UTIL_H
#define WEBMLIVE_FILE_UTIL_H

#pragma once

#include <string>
#include "chromium/base/basictypes.h"

namespace WebmLive {
namespace FileUtil {

bool file_exists(std::wstring path);
uint64 get_file_size(std::wstring path);

} // FileUtil
} // WebmLive

#endif // WEBMLIVE_FILE_UTIL_H
