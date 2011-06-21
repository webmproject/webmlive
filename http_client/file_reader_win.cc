// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "http_client_base.h"
#include "file_reader.h"

#include <shlwapi.h>

#include <cstdio>
#include <sstream>

#include "debug_util.h"

namespace WebmLive {

class FileReaderImpl {
public:
  FileReaderImpl();
  ~FileReaderImpl();
  int Init(std::wstring file_name);
  int Read(size_t num_bytes, void* ptr_buffer, size_t* ptr_num_read);
  int64 GetBytesRead() const { return bytes_read_; };
private:
  FILE* ptr_file_;
  int64 bytes_read_;
  DISALLOW_COPY_AND_ASSIGN(FileReaderImpl);
};

FileReaderImpl::FileReaderImpl():
  bytes_read_(0),
  ptr_file_(NULL)
{
}

FileReaderImpl::~FileReaderImpl()
{
  if (ptr_file_) {
    fclose(ptr_file_);
    ptr_file_ = NULL;
    bytes_read_ = 0;
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
  errno_t err = _wfopen_s(&ptr_file_, file_name.c_str(), L"rb");
  if (err) {
    int err_gle = GetLastError();
    DBGLOG("ERROR: could not open file, err=" << err << " GetLastError="
           << err_gle);
    return err_gle;
  }
  return ERROR_SUCCESS;
}

int FileReaderImpl::Read(size_t num_bytes, void* ptr_buffer,
                         size_t* ptr_num_read)
{
  if (num_bytes < 1 || !ptr_buffer || !ptr_num_read) {
    return E_INVALIDARG;
  }
  size_t& num_read = *ptr_num_read;
  num_read = fread(ptr_buffer, 1, num_bytes, ptr_file_);
  int status = ERROR_SUCCESS;
  if (num_bytes != num_read) {
    DBGLOG("shortfall! requested=" << num_bytes << " read=" << num_read);
    status = ERROR_HANDLE_EOF;
  }
  bytes_read_ += num_read;
  return status;
}

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
  file_name_ = file_name;
  ptr_reader_.reset(new (std::nothrow) FileReaderImpl());
  if (!ptr_reader_) {
    DBGLOG("ERROR: can't construct FileReaderImpl.");
    return E_OUTOFMEMORY;
  }
  return ptr_reader_->Init(file_name);
}

int64 FileReader::GetBytesAvailable() const
{
  int64 bytes_available = 0;
  WIN32_FILE_ATTRIBUTE_DATA attr = {0};
  if (GetFileAttributesEx(file_name_.c_str(), GetFileExInfoStandard, &attr)) {
    bytes_available = (attr.nFileSizeHigh << 4) + attr.nFileSizeLow;
    bytes_available -= ptr_reader_->GetBytesRead();
  }
  return bytes_available;
}

int FileReader::Read(size_t num_bytes, void* ptr_buffer, size_t* ptr_num_read)
{
  return ptr_reader_->Read(num_bytes, ptr_buffer, ptr_num_read);
}

} // WebmLive
