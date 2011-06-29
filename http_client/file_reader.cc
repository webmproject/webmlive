// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "http_client_base.h"
#include "file_reader.h"

#ifdef _WIN32
#include "file_reader_win.h"
#endif

#include <cstdio>
#include <sstream>

#include "debug_util.h"

namespace WebmLive {

FileReader::FileReader()
{
}

FileReader::~FileReader()
{
}

int FileReader::Init(std::string file_name)
{
  std::wostringstream fname_cnv;
  fname_cnv << file_name.c_str();
  return Init(fname_cnv.str());
}

int FileReader::Init(std::wstring file_name)
{
  ptr_reader_.reset(new (std::nothrow) FileReaderImpl());
  if (!ptr_reader_) {
    DBGLOG("ERROR: can't construct FileReaderImpl.");
    return kOpenFailed;
  }
  return ptr_reader_->Init(file_name);
}

uint64 FileReader::GetBytesAvailable() const
{
  return ptr_reader_->GetBytesAvailable();
}

int FileReader::Read(size_t num_bytes, void* ptr_buffer, size_t* ptr_num_read)
{
  return ptr_reader_->Read(num_bytes, ptr_buffer, ptr_num_read);
}

} // WebmLive
