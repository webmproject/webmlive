// Copyright (c) 2015 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "encoder/time_util.h"

#include "glog/logging.h"

namespace webmlive {

const size_t kTimeUtilBufferSize = 256;

std::string LocalDateString() {
  char date_buffer[kTimeUtilBufferSize] = {0};
  // %Y - year
  // %m - month, zero padded (01-12)
  // %d - day of month, zero padded (01-31).
  const char format_string[] = "%Y%m%d";
  const time_t raw_time_now = time(NULL);
  const struct tm* time_now = localtime(&raw_time_now);

  if (strftime(&date_buffer[0], sizeof(date_buffer), format_string,
               time_now) == 0) {
    LOG(ERROR) << "DateString failed..";
    return "";
  }

  const std::string date_string = date_buffer;
  return date_string;
}

std::string LocalTimeString() {
  char time_buffer[kTimeUtilBufferSize] = {0};
  // %H - hour, zero padded, 24 hour clock (00-23)
  // %M - minute, zero padded (00-59)
  // %S - second, zero padded (00-61)
  const char format_string[] = "%H%M%S";
  const time_t raw_time_now = time(NULL);
  const struct tm* time_now = localtime(&raw_time_now);

  if (strftime(&time_buffer[0], sizeof(time_buffer), format_string,
               time_now) == 0) {
    LOG(ERROR) << "TimeString failed.";
    return "";
  }

  const std::string time_string = time_buffer;
  return time_string;
}

std::string StrFTime(const struct tm* time_value,
                     const std::string& format_string) {
  char strftime_buffer[kTimeUtilBufferSize] = {0};
  if (strftime(&strftime_buffer[0], sizeof(strftime_buffer),
               format_string.c_str(), time_value) == 0) {
    LOG(ERROR) << "StrFTime failed.";
    return "";
  }

  const std::string out_string = strftime_buffer;
  return out_string;
}

}  // namespace webmlive
