// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef WEBMLIVE_FILE_READER_WIN_H
#define WEBMLIVE_FILE_READER_WIN_H

#pragma once

#include "http_client_base.h"

#include <fstream>
#include <string>

#include "chromium/base/basictypes.h"

namespace WebmLive {

class FileReaderImpl {
 public:
  enum {
    kSuccess = 0,
  };
  FileReaderImpl();
  ~FileReaderImpl();
  int CreateFile(std::wstring file_name);
  int Read(size_t num_bytes, uint8* ptr_buffer, size_t* ptr_num_read);
  uint64 GetBytesAvailable();
  int64 GetBytesRead() const { return bytes_read_; };
 private:
  int Open();
  int ReadFromStream(size_t num_bytes, uint8* ptr_buffer,
                     size_t& num_read);
  //std::ifstream input_file_;
  FILE* ptr_file_;
  int64 bytes_read_;
  std::wstring file_name_;
  DISALLOW_COPY_AND_ASSIGN(FileReaderImpl);
};

}

#endif // WEBMLIVE_FILE_READER_WIN_H
