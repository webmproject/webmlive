// Copyright (c) 2015 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "encoder/win/string_util_win.h"

#include <memory>
#include <sstream>

#include "glog/logging.h"

namespace webmlive {

// Converts a std::string to std::wstring.
std::wstring StringToWString(const std::string& str) {
  std::wostringstream wstr;
  wstr << str.c_str();
  return wstr.str();
}

// Converts |wstr| to a multi-byte string and returns result std::string.
std::string WStringToString(const std::wstring& wstr) {
  // Conversion buffer for |wcstombs| calls.
  const size_t buf_size = wstr.length() + 1;
  std::unique_ptr<char[]> temp_str(
      new (std::nothrow) char[buf_size]);  // NOLINT
  if (!temp_str) {
    LOG(ERROR) << "can't convert wstring of length=" << wstr.length();
    return "";
  }
  memset(temp_str.get(), 0, buf_size);
  size_t num_converted = 0;
  if (wcstombs_s(&num_converted, temp_str.get(), buf_size, wstr.c_str(),
                 wstr.length() * sizeof(wchar_t))) {
    LOG(ERROR) << "conversion failed.";
    return "";
  }
  std::string str = temp_str.get();
  return str;
}

}  // namespace webmlive