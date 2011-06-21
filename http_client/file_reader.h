// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef WEBMLIVE_FILE_READER_H
#define WEBMLIVE_FILE_READER_H

#pragma once

#include "boost/scoped_ptr.hpp"
#include "chromium/base/basictypes.h"
#include <string>

namespace WebmLive {

class FileReaderImpl;

class FileReader {
public:
  FileReader();
  ~FileReader();
  int Init(std::string file_name);
  int Init(std::wstring file_name);
  int64 GetBytesAvailable() const;
  int Read(size_t num_bytes, void* ptr_buffer, size_t* ptr_num_read);
private:
  boost::scoped_ptr<FileReaderImpl> ptr_reader_;
  std::wstring file_name_;
  DISALLOW_COPY_AND_ASSIGN(FileReader);
};

} // WebmLive

#endif // WEBMLIVE_FILE_READER_H
