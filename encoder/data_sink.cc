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

void DataSink::AddDataSink(DataSinkInterface* data_sink) {
  std::lock_guard<std::mutex> lock(mutex_);
  data_sinks_.push_back(data_sink);
}

bool DataSink::WriteData(const std::string& id,
                         const uint8* ptr_data, int data_length) {
  std::lock_guard<std::mutex> lock(mutex_);
  DataSinkInterface::SharedDataSinkBuffer buffer;
  buffer.reset(new (std::nothrow) DataSinkBuffer);

  if (!buffer.get()) {
    LOG(ERROR) << "Out of memory.";
    return false;
  }

  buffer->data.assign(ptr_data, ptr_data + data_length);
  buffer->id = id;

  for (auto data_sink : data_sinks_) {
    bool write_ok = data_sink->WriteData(buffer);
    if (!write_ok) {
      // Log and ignore the error.
      LOG(ERROR) << "WriteData failed on sink with name: " << data_sink->Name();
    }
  }

  return true;
}

}  // namespace webmlive
