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

#include <sstream>

#include "debug_util.h"
#include "file_util.h"

namespace WebmLive {

FileReaderImpl::FileReaderImpl()
    : bytes_read_(0)
{
}

FileReaderImpl::~FileReaderImpl()
{
  if (input_file_.is_open()) {
    input_file_.close();
  }
}

int FileReaderImpl::Init(std::wstring file_name)
{
  if (file_name.empty()) {
    DBGLOG("ERROR: empty file_name");
    return E_INVALIDARG;
  }
  if (!FileUtil::file_exists(file_name)) {
    DBGLOG("ERROR: file " << file_name.c_str() << " does not exist.");
    return ERROR_FILE_NOT_FOUND;
  }
  // Make sure the file exists-- we'll be reopening it on each call to |Read|
  using std::ios_base;
  file_name_ = file_name;
  if (Open()) {
    DBGLOG("ERROR: could not open file, GetLastError=" << GetLastError());
    return FileReader::kOpenFailed;
  }
  input_file_.close();
  return kSuccess;
}

int FileReaderImpl::Read(size_t num_bytes, void* ptr_buffer,
                         size_t* ptr_num_read)
{
  if (num_bytes < 1 || !ptr_buffer || !ptr_num_read) {
    return E_INVALIDARG;
  }
  size_t& num_read = *ptr_num_read;
  char* ptr_buf = reinterpret_cast<char*>(ptr_buffer);

  if (GetBytesAvailable() > 0) {
    int err = OpenAtReadOffset();
    if (err) {
      DBGLOG("ERROR: could not open file, err=" << err << " GetLastError="
              << GetLastError());
      return FileReader::kOpenFailed;
    }
    input_file_.read(ptr_buf, num_bytes);
    const bool read_failed = input_file_.bad() || input_file_.fail();
    input_file_.close();
    if (read_failed) {
      DBGLOG("ERROR: read error, badbit set, GetLastError=" << GetLastError());
      return FileReader::kReadFailed;
    }
    num_read = input_file_.gcount();
    bytes_read_ += num_read;
    if (num_bytes != num_read) {
      DBGLOG("shortfall! requested=" << num_bytes << " read=" << num_read);
      return FileReader::kAtEOF;
    }
  }
  return kSuccess;
}

uint64 FileReaderImpl::GetBytesAvailable() const
{
  uint64 bytes_available = FileUtil::get_file_size(file_name_);
  if (bytes_available > 0) {
    bytes_available -= bytes_read_;
  }
  return bytes_available;
}

int FileReaderImpl::Open()
{
  using std::ios_base;
  input_file_.open(file_name_.c_str(), ios_base::in | ios_base::binary);
  if (!input_file_.is_open() || input_file_.fail()) {
    DBGLOG("ERROR: could not open file, GetLastError=" << GetLastError());
    return FileReader::kOpenFailed;
  }
  return kSuccess;
}

int FileReaderImpl::OpenAtReadOffset()
{
  if (Open()) {
    DBGLOG("ERROR: could not open file, GetLastError=" << GetLastError());
    return FileReader::kOpenFailed;
  }
  if (bytes_read_ > 0) {
    using std::ios_base;
    // TODO(tomfinegan): this breaks once we get to 2GB, use boost maybe?
    long offset = static_cast<long>(bytes_read_);
    input_file_.seekg(offset, ios_base::beg);
    if (input_file_.fail() || input_file_.bad()) {
      DBGLOG("ERROR: could not seek to read offset, GetLastError="
             << GetLastError());
      input_file_.close();
      return kSeekFailed;
    }
  }
  return kSuccess;
}

} // WebmLive
