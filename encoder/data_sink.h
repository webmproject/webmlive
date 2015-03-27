// Copyright (c) 2012 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef WEBMLIVE_ENCODER_DATA_SINK_H_
#define WEBMLIVE_ENCODER_DATA_SINK_H_

#include <string>

#include "encoder/basictypes.h"

namespace webmlive {

class DataSinkInterface {
 public:
  virtual ~DataSinkInterface() {}

  // Returns true when the class implementing |DataSinkInterface| is ready to
  // receive data via a call to |WriteData()|.
  virtual bool Ready() const = 0;

  // Writes data to the sink and returns true when successful.
  virtual bool WriteData(const std::string& id,
                         const uint8* ptr_data, int data_length) = 0;
};

}  // namespace webmlive

#endif  // WEBMLIVE_ENCODER_DATA_SINK_H_
