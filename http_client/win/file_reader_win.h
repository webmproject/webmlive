// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef HTTP_CLIENT_WIN_FILE_READER_WIN_H_
#define HTTP_CLIENT_WIN_FILE_READER_WIN_H_

#include <cstdio>
#include <string>

#include "http_client/basictypes.h"
#include "http_client/file_reader.h"
#include "http_client/http_client_base.h"

namespace webmlive {

// FileReader implementation.  Creates a writable file that will later be
// read through calls to the public |Read| method.
class FileReaderImpl {
 public:
  // Define |FileReader| status codes for brevity.
  enum {
    // Status codes.
    kSeekFailed = FileReader::kSeekFailed,
    kOpenFailed = FileReader::kOpenFailed,
    kReadFailed = FileReader::kReadFailed,
    kInvalidArg = FileReader::kInvalidArg,
    kSuccess = FileReader::kSuccess,
    kAtEOF = FileReader::kAtEOF,
    // Values for use with the |mode| argument of |Open|.
    kModeCreate,
    kModeOpen,
  };
  FileReaderImpl();
  ~FileReaderImpl();
  // Create file specified with the |file_name| parameter.  Overwrites existing
  // files.
  int CreateFile(std::wstring file_path);
  // Read up to |num_bytes| into |ptr_buffer|.  Actual number of bytes read
  // returned through |ptr_num_read|.  Calls |ReadFromStream| to do the actual
  // work.  Returns |kAtEOF| if end of file is reached during the read.
  int Read(size_t num_bytes, uint8* ptr_buffer, size_t* ptr_num_read);
 private:
  // Creates or reopens the file based based on |mode| value.
  int Open(int mode);
  // Read up to |num_bytes| into |ptr_buffer|.  Actual number of bytes read
  // returned through |num_read|.
  int ReadFromStream(size_t num_bytes, uint8* ptr_buffer,
                     size_t& num_read);
  // Returns number of unread bytes remaining in |ptr_file_|.
  uint64 GetBytesAvailable();
  // File pointer.
  FILE* ptr_file_;
  // Total bytes read.
  int64 bytes_read_;
  // File name.
  std::wstring file_path_;
  WEBMLIVE_DISALLOW_COPY_AND_ASSIGN(FileReaderImpl);
};

}  // namespace webmlive

#endif  // HTTP_CLIENT_WIN_FILE_READER_WIN_H_
