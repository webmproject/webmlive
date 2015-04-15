// Copyright (c) 2012 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef WEBMLIVE_ENCODER_DATA_SINK_H_
#define WEBMLIVE_ENCODER_DATA_SINK_H_

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "encoder/basictypes.h"

namespace webmlive {

struct DataSinkBuffer {
  std::string id;
  std::vector<uint8> data;
};

class DataSinkInterface {
 public:
  typedef std::shared_ptr<DataSinkBuffer> SharedDataSinkBuffer;
  virtual ~DataSinkInterface() {}
  virtual bool WriteData(SharedDataSinkBuffer buffer) = 0;
  virtual std::string Name() const = 0;
};

class DataSink {
 public:
  DataSink() {}
  ~DataSink() {}

  // Adds |data_sink| to |data_sinks_|.
  void AddDataSink(DataSinkInterface* data_sink);

  // Writes |id| and |ptr_data| to all data sinks in |data_sinks_|. Returns
  // true when the data has been sent to all sinks.
  bool WriteData(const std::string& id, const uint8* ptr_data, int data_length);

 private:
  std::mutex mutex_;
  std::vector<DataSinkInterface*> data_sinks_;
};

}  // namespace webmlive

#endif  // WEBMLIVE_ENCODER_DATA_SINK_H_
