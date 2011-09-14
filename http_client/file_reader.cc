// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "file_reader.h"

#include <cstdio>
#include <sstream>
#include <string>

#include "glog/logging.h"
#ifdef _WIN32
#include "win/file_reader_win.h"
#endif

namespace webmlive {

FileReader::FileReader() {
}

FileReader::~FileReader() {
}

// Convert |file_path| to wide chars using a |wostringstream|, and then call
// |CreateFile|.
int FileReader::CreateFile(std::string file_path) {
  std::wostringstream fpath_cnv;
  fpath_cnv << file_path.c_str();
  return CreateFile(fpath_cnv.str());
}

// Construct |ptr_reader_| and call its |CreateFile| method.
int FileReader::CreateFile(std::wstring file_path) {
  ptr_reader_.reset(new (std::nothrow) FileReaderImpl());
  if (!ptr_reader_) {
    LOG(ERROR) << "ERROR: can't construct FileReaderImpl.";
    return kOpenFailed;
  }
  return ptr_reader_->CreateFile(file_path);
}

// Pass through to implementation defined |Read| method.
int FileReader::Read(size_t num_bytes, uint8* ptr_buffer,
                     size_t* ptr_num_read) {
  return ptr_reader_->Read(num_bytes, ptr_buffer, ptr_num_read);
}

}  // namespace webmlive
