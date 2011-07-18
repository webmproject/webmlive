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

#include "http_client_base.h"
#include "boost/scoped_ptr.hpp"
#include "chromium/base/basictypes.h"

namespace WebmLive {

class FileReaderImpl;

// Pimpl idiom based file reader. Intended to allow for platform specific
// reader implementations.  This reader is a little different-- it creates a
// file that is than used for reading the output of a live WebM encode.
class FileReader {
 public:
  enum {
    // Status codes returns by reader methods.
    // Seek operation on the input file failed.
    kSeekFailed = -5,
    // Unable to create or open the input file.
    kOpenFailed = -3,
    // Unable to read data from the input file.
    kReadFailed = -2,
    // Invalid argument supplied to method.
    kInvalidArg = -1,
    // Success.
    kSuccess = 0,
    // Reached end of file while reading.
    kAtEOF = 1,
  };
  FileReader();
  ~FileReader();
  // Converts |file_path| to wide chars and calls |CreateFile|.
  int CreateFile(std::string file_path);
  // Creates |ptr_reader_| and passes it |file_path|.
  int CreateFile(std::wstring file_path);
  // Calls |Read| on |ptr_reader_|.
  int Read(size_t num_bytes, uint8* ptr_buffer, size_t* ptr_num_read);
 private:
   // Pointer to reader implementation.
  boost::scoped_ptr<FileReaderImpl> ptr_reader_;
  DISALLOW_COPY_AND_ASSIGN(FileReader);
};

} // WebmLive

#endif // WEBMLIVE_FILE_READER_H
