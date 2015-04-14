// Copyright (c) 2015 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef WEBMLIVE_ENCODER_FILE_WRITER_H_
#define WEBMLIVE_ENCODER_BUFFER_UTIL_H_

#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "encoder/data_sink.h"

namespace webmlive {

// Writes SharedDataSinkBuffer contents to file(s). When in DASH mode writes
// are to multiple files named according to the |id| member of the
// SharedDataSinkBuffer. Otherwise writes all SharedDataSinkBuffer contents to
// a single file.
class FileWriter : public DataSinkInterface2 {
 public:
  FileWriter() : dash_mode_(true) {}
  virtual ~FileWriter() {}
  
  // Readies the writer and returns true. Must be called before Run().
  bool Init(bool dash_mode);

  // Runs the writer thread and returns true.
  bool Run();

  // DataSinkInferface2 methods.
  virtual bool WriteData(SharedDataSinkBuffer buffer) override;
  virtual std::string Name() const override { return "FileWriter"; }

 private:
  void WriterThread();
  bool WriteFile();

  bool dash_mode_;
  std::string file_name_;  // Used only when |dash_mode_| is false.
  std::mutex mutex_;
  std::condition_variable wait_condition_;
  std::shared_ptr<std::thread> thread_;
};

}  // namespace webmlive

#endif  // WEBMLIVE_ENCODER_FILE_WRITER_H_