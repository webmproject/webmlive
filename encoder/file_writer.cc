// Copyright (c) 2015 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "encoder/file_writer.h"

#include <condition_variable>
#include <ctime>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "glog/logging.h"

namespace webmlive {

bool FileWriter::Init(bool dash_mode) {
  if (!dash_mode) {
    char file_name[30] = {0};
    // %Y - year
    // %m - month, zero padded (01-12)
    // %d - day of month, zero padded (01-31).
    // %H - hour, zero padded, 24 hour clock (00-23)
    // %M - minute, zero padded (00-59)
    // %S - second, zero padded (00-61)
    const char format_string[] = "%Y%m%d%H%M%S";
    const time_t raw_time_now = time(NULL);
    const struct tm* time_now = localtime(&raw_time_now);

    if (strftime(&file_name[0], sizeof file_name, format_string,
                 time_now) == 0) {
      LOG(ERROR) << "FileWriter cannot generate file name.";
      return false;
    }

    file_name_ = file_name;
    file_name_ += ".webm";
  }

  return true;
}

bool FileWriter::Run() {
  using std::bind;
  using std::shared_ptr;
  using std::thread;
  using std::nothrow;
  thread_ = shared_ptr<thread>(
      new (nothrow) thread(bind(&FileWriter::WriterThread,  // NOLINT
                                this)));
  if (!thread_) {
    LOG(ERROR) << "Out of memory.";
    return false;
  }
  return true;
}

bool WriteData(DataSinkInterface2::SharedDataSinkBuffer buffer) {
  // TODO(tomfinegan): Copying this is not necessary; SharedDataBufferQueue or
  // something should be provided by data_sink.h.

  std::vector 
}

}  // namespace webmlive
