// Copyright (c) 2015 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "encoder/data_sink.h"

#include <memory>
#include <mutex>

#include "glog/logging.h"

namespace webmlive {

//
// SharedBufferQueue
//
bool SharedBufferQueue::EnqueueBuffer(const SharedDataSinkBuffer& buffer) {
  if (buffer.get() == NULL) {
    LOG(ERROR) << "Empty SharedDataSinkBuffer.";
    return false;
  }
  std::lock_guard<std::mutex> lock(mutex_);
  buffer_q_.push(buffer);
  return true;
}

SharedDataSinkBuffer SharedBufferQueue::DequeueBuffer() {
  SharedDataSinkBuffer buffer;
  std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
  if (lock.owns_lock() && !buffer_q_.empty()) {
    buffer = buffer_q_.front();
    buffer_q_.pop();
  }
  return buffer;
}

size_t SharedBufferQueue::GetNumBuffers() {
  std::lock_guard<std::mutex> lock(mutex_);
  return buffer_q_.size();
}

//
// DataSink
//
void DataSink::AddDataSink(DataSinkInterface* data_sink) {
  std::lock_guard<std::mutex> lock(mutex_);
  data_sinks_.push_back(data_sink);
}

bool DataSink::WriteData(const std::string& id,
                         const uint8* ptr_data, int data_length) {
  std::lock_guard<std::mutex> lock(mutex_);
  SharedDataSinkBuffer buffer;
  buffer.reset(new (std::nothrow) DataSinkBuffer);

  if (!buffer.get()) {
    LOG(ERROR) << "Out of memory.";
    return false;
  }

  buffer->data.assign(ptr_data, ptr_data + data_length);
  buffer->id = id;

  for (auto data_sink : data_sinks_) {
    if (!data_sink->WriteData(buffer)) {
      // Log and ignore the error.
      LOG(ERROR) << "WriteData failed on sink with name: " << data_sink->Name();
    }
  }

  return true;
}

}  // namespace webmlive
