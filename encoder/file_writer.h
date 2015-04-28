// Copyright (c) 2015 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef WEBMLIVE_ENCODER_FILE_WRITER_H_
#define WEBMLIVE_ENCODER_FILE_WRITER_H_

#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "encoder/buffer_util.h"
#include "encoder/data_sink.h"

namespace webmlive {

// Writes SharedDataSinkBuffer contents to file(s). When in DASH mode writes
// are to multiple files named according to the |id| member of the
// SharedDataSinkBuffer. Otherwise writes all SharedDataSinkBuffer contents to
// a single file.
class FileWriter : public DataSinkInterface {
 public:
  FileWriter() : dash_mode_(true), stop_(false) {}
  virtual ~FileWriter() {}

  // Readies the writer and returns true. Must be called before Run().
  bool Init(bool dash_mode, const std::string& directory);

  // Runs the writer thread and returns true upon success.
  bool Run();

  // Stops the writer thread. Blocks until thread stops. Returns true upon
  // success.
  bool Stop();

  // DataSinkInferface methods.
  bool WriteData(const SharedDataSinkBuffer& buffer) override;
  std::string Name() const override { return "FileWriter"; }

 private:
  bool StopRequested();
  void WaitForUserData();
  bool WriteFile(const SharedDataSinkBuffer& buffer) const;
  void WriterThread();

  bool dash_mode_;
  bool stop_;
  std::string directory_;
  std::string file_name_;  // Used only when |dash_mode_| is false.
  std::mutex mutex_;
  std::condition_variable wake_condition_;
  std::shared_ptr<std::thread> thread_;
  SharedBufferQueue buffer_q_;
};

}  // namespace webmlive

#endif  // WEBMLIVE_ENCODER_FILE_WRITER_H_