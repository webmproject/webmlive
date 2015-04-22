// Copyright (c) 2015 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef WEBMLIVE_ENCODER_TIME_UTIL_H_
#define WEBMLIVE_ENCODER_TIME_UTIL_H_

#include <ctime>
#include <string>

namespace webmlive {

// Time string utility functions. All return the string noted or an empty string
// upon failure.

// Returns current date in the format: YYYYMMDD.
std::string LocalDateString();

// Returns current time in the format: HHMMSS.
std::string LocalTimeString();

// Returns user requested time string.
std::string StrFTime(const struct tm* time_value,
                     const std::string& format_string);

}  // namespace webmlive

#endif  // WEBMLIVE_ENCODER_TIME_UTIL_H_