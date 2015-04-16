// Copyright (c) 2015 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "encoder/file_writer.h"

#include <condition_variable>
#include <cstdio>
#include <ctime>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "glog/logging.h"

namespace webmlive {

bool FileWriter::Init(bool dash_mode, const std::string& directory) {
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

    if (strftime(&file_name[0], sizeof(file_name), format_string,
                 time_now) == 0) {
      LOG(ERROR) << "FileWriter cannot generate file name.";
      return false;
    }

    file_name_ = file_name;
    file_name_ += ".webm";
  }
  directory_ = directory;
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

bool FileWriter::Stop() {
  // Tell WriterThread() to stop.
  mutex_.lock();
  stop_ = true;
  mutex_.unlock();

  // Wake and wait for WriterThread() to exit.
  wake_condition_.notify_one();
  thread_->join();
  return true;
}

// Copies data into |buffer_q_| and returns true.
bool FileWriter::WriteData(DataSinkInterface::SharedDataSinkBuffer buffer) {
  // TODO(tomfinegan): Copying data is not necessary; SharedDataBufferQueue or
  // something should be provided by data_sink.h. Abusing BufferQueue
  // temporarily since it just works and incoming buffers aren't that large.
  if (!buffer_q_.EnqueueBuffer(buffer->id,
                               &buffer->data[0], buffer->data.size())) {
    LOG(ERROR) << "Write buffer enqueue failed.";
    return false;
  }
  // Wake WriterThread().
  LOG(INFO) << "waking WriterThread with " << buffer->data.size() << " bytes";
  wake_condition_.notify_one();
  return true;
}

// Try to obtain lock on |mutex_|, and return the value of |stop_| if lock is
// obtained.  Returns false if unable to obtain the lock.
bool FileWriter::StopRequested() {
  bool stop_requested = false;
  std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
  if (lock.owns_lock()) {
    stop_requested = stop_;
  }
  return stop_requested;
}

// Idle the upload thread while awaiting user data.
void FileWriter::WaitForUserData() {
  std::unique_lock<std::mutex> lock(mutex_);
  wake_condition_.wait(lock);  // Unlock |mutex_| and idle the thread while we
                               // wait for the next chunk of user data.
}

// Writes |data| contents to file and returns true upon success.
bool FileWriter::WriteFile(const BufferQueue::Buffer& data) const {
  std::string file_name;
  if (dash_mode_) {
    file_name = directory_ + data.id;
  } else {
    file_name = directory_ + file_name_;
  }
  FILE* file = fopen(file_name.c_str(), "ab");
  if (!file) {
    LOG(ERROR) << "Unable to open output file.";
    return false;
  }
  const size_t bytes_written =
      fwrite(reinterpret_cast<const void*>(&data.data[0]),
             1, data.data.size(), file);
  fclose(file);
  return (bytes_written == data.data.size());
}

// Runs until StopRequested() returns true.
void FileWriter::WriterThread() {
  while (!StopRequested() || buffer_q_.GetNumBuffers() > 0) {
    const BufferQueue::Buffer* buffer = buffer_q_.DequeueBuffer();
    if (!buffer) {
      // Wait for a buffer.
      WaitForUserData();
      continue;
    }
    if (!WriteFile(*buffer)) {
      LOG(ERROR) << "Write failed for id: " << buffer->id;
    }
  }
}

}  // namespace webmlive
