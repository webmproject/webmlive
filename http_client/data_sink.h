// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef HTTP_CLIENT_DATA_SINK_H_
#define HTTP_CLIENT_DATA_SINK_H_

#include "http_client/basictypes.h"

namespace webmlive {

class DataSinkInterface {
 public:
  virtual ~DataSinkInterface() = 0;

  // Returns true when the class implementing |DataSinkInterface| is ready to
  // receive data via a call to |WriteData()|.
  virtual bool Ready() const = 0;

  // Writes data to the sink and returns true when successful.
  virtual bool WriteData(const uint8* ptr_data, int32 data_length) = 0;
};

}  // end namespace webmlive

#endif  // HTTP_CLIENT_DATA_SINK_H_
