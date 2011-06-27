// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "http_client_base.h"
#include "file_reader.h"
#include "file_reader_win.h"

#include <shlwapi.h>

#include <cstdio>
#include <sstream>

#include "debug_util.h"

namespace WebmLive {

FileReaderImpl::FileReaderImpl():
  bytes_read_(0),
  file_hndl_(INVALID_HANDLE_VALUE)
{
}

FileReaderImpl::~FileReaderImpl()
{
  if (file_hndl_ != INVALID_HANDLE_VALUE) {
    CloseHandle(file_hndl_);
    file_hndl_ = INVALID_HANDLE_VALUE;
  }
}

int FileReaderImpl::Init(std::wstring file_name)
{
  if (file_name.empty()) {
    DBGLOG("ERROR: empty file_name");
    return E_INVALIDARG;
  }
  if (PathFileExists(file_name.c_str()) != TRUE) {
    DBGLOG("ERROR: file " << file_name.c_str() << " does not exist.");
    return ERROR_FILE_NOT_FOUND;
  }
  file_hndl_ = CreateFile(file_name.c_str(), GENERIC_READ, FILE_SHARE_READ,
                          NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (file_hndl_ == INVALID_HANDLE_VALUE) {
    DBGLOG("ERROR: could not open file, GetLastError=" << GetLastError());
    return FileReader::kOpenFailed;
  }
  file_name_ = file_name;
  return FileReader::kSuccess;
}

int FileReaderImpl::Read(size_t num_bytes, void* ptr_buffer,
                         size_t* ptr_num_read)
{
  if (num_bytes < 1 || !ptr_buffer || !ptr_num_read) {
    return E_INVALIDARG;
  }
  size_t& num_read = *ptr_num_read;
  DWORD dw_bytes_read = 0;
  BOOL read_ok = ReadFile(file_hndl_, ptr_buffer, num_bytes, &dw_bytes_read,
                          NULL);
  if (!read_ok) {
    DBGLOG("ERROR: could not read file, GetLastError=" << GetLastError());
    return FileReader::kReadFailed;
  }
  num_read = dw_bytes_read;
  bytes_read_ += num_read;
  int status = FileReader::kSuccess;
  if (num_bytes != num_read) {
    DBGLOG("shortfall! requested=" << num_bytes << " read=" << num_read);
    status = FileReader::kAtEOF;
  }
  return status;
}

int64 FileReaderImpl::GetBytesAvailable() const
{
  int64 bytes_available = 0;
  WIN32_FILE_ATTRIBUTE_DATA attr = {0};
  if (GetFileAttributesEx(file_name_.c_str(), GetFileExInfoStandard, &attr)) {
    bytes_available = (attr.nFileSizeHigh << 4) + attr.nFileSizeLow;
    bytes_available -= bytes_read_;
  }
  return bytes_available;
}

} // WebmLive
