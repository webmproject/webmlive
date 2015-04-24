// Copyright (c) 2015 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef WEBMLIVE_ENCODER_WIN_STRING_UTIL_WIN_H_
#define WEBMLIVE_ENCODER_WIN_STRING_UTIL_WIN_H_

#include <string>

namespace webmlive {

// String utility functions. All return an empty string upon error.

// Converts and returns the contents of a std::string as a std::wstring.
std::wstring StringToWString(const std::string& str);

// Converts and returns the contents of a std::wstring as a std::string.
std::string WStringToString(const std::wstring& wstr);

}  // namespace webmlive

#endif  // WEBMLIVE_ENCODER_WIN_STRING_UTIL_WIN_H_