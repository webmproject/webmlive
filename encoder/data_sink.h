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

class DataSinkInterface {
 public:
  virtual ~DataSinkInterface() {}

  // Writes data to the sink and returns true when successful.
  virtual bool WriteData(const std::string& id,
                         const uint8* ptr_data, int data_length) = 0;
};

class DataSinkInterface2 {
 public:
  struct Buffer {
    typedef std::shared_ptr<std::vector<uint8_t>> SharedDataPtr;
    std::string id;
    SharedDataPtr data;
  };

  virtual ~DataSinkInterface2() {}
  virtual bool WriteData(const Buffer& buffer);
};

class DataSink : public DataSinkInterface {
 public:
  DataSink() {}
  virtual ~DataSink() {}
  
  // Adds |data_sink| to |data_sinks_|.
  void AddDataSink(DataSink* data_sink);

  virtual bool WriteData(const std::string& id,
                         const uint8* ptr_data, int data_length) override;
 private:
  std::mutex mutex_;
  std::vector<DataSink*> data_sinks_;
};

}  // namespace webmlive

#endif  // WEBMLIVE_ENCODER_DATA_SINK_H_
