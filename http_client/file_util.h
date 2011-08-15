// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef HTTP_CLIENT_FILE_UTIL_H_
#define HTTP_CLIENT_FILE_UTIL_H_

#include <string>

#include "basictypes.h"

namespace webmlive {
namespace FileUtil {

bool file_exists(std::string path);
uint64 get_file_size(std::string path);

}  // FileUtil
}  // webmlive

#endif  // HTTP_CLIENT_FILE_UTIL_H_
