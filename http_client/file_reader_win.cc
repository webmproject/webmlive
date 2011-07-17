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

#include <share.h> // defines _SH_DENYNO, used with _wfsopen

#include <sstream>

#include "debug_util.h"
#include "file_util.h"

namespace WebmLive {
namespace {
  const wchar_t* const kModeOpen = L"w+b";
  const wchar_t* const kModeReopen = L"r+b";
}

FileReaderImpl::FileReaderImpl() : bytes_read_(0), ptr_file_(NULL) {
}

FileReaderImpl::~FileReaderImpl() {
  fclose(ptr_file_);
  ptr_file_ = NULL;
}

int FileReaderImpl::CreateFile(std::wstring file_name) {
  if (file_name.empty()) {
    DBGLOG("ERROR: empty file_name");
    return FileReader::kOpenFailed;
  }
  file_name_ = file_name;
  if (Open(kModeOpen)) {
    DBGLOG("ERROR: could not open file, GetLastError=" << GetLastError());
    return FileReader::kOpenFailed;
  }
  return kSuccess;
}

int FileReaderImpl::Read(size_t num_bytes, uint8* ptr_buffer,
                         size_t* ptr_num_read) {
  if (num_bytes < 1 || !ptr_buffer || !ptr_num_read) {
    return FileReader::kInvalidArg;
  }
  size_t& num_read = *ptr_num_read;
  int status = Open(kModeReopen);
  if (status) {
    DBGLOG("ERROR: could not open file, GetLastError=" << GetLastError());
    return FileReader::kOpenFailed;
  }
  if (GetBytesAvailable() > 0) {
    int err = ReadFromStream(num_bytes, ptr_buffer, num_read);
    if (err && err != FileReader::kAtEOF) {
      DBGLOG("ERROR: could not read file, err=" << err);
      return err;
    }
    bytes_read_ += num_read;
    if (num_bytes != num_read) {
      DBGLOG("shortfall! requested=" << num_bytes << " read=" << num_read);
      return FileReader::kAtEOF;
    }
  }
  return kSuccess;
}

int FileReaderImpl::Open(const wchar_t* const ptr_mode) {
  if (ptr_file_) {
    DBGLOG("closing");
    fclose(ptr_file_);
    ptr_file_ = NULL;
  }
  DBGLOG("opening, mode=" << ptr_mode);
  ptr_file_ = _wfsopen(file_name_.c_str(), ptr_mode, _SH_DENYNO);
  if (!ptr_file_) {
    DBGLOG("ERROR: could not open file, GetLastError=" << GetLastError());
    return FileReader::kOpenFailed;
  }
  if (bytes_read_ > 0) {
    if (_fseeki64(ptr_file_, bytes_read_, SEEK_SET)) {
      DBGLOG("ERROR: could not seek to read pos, GetLastError="
             << GetLastError());
      DBGLOG("closing");
      fclose(ptr_file_);
      ptr_file_ = NULL;
      return FileReader::kSeekFailed;
    }
  }
  return kSuccess;
}

int FileReaderImpl::ReadFromStream(size_t num_bytes, uint8* ptr_buffer,
                                   size_t& num_read)
{
  num_read = fread(ptr_buffer, sizeof(uint8), num_bytes, ptr_file_);
  if (feof(ptr_file_)) {
    clearerr(ptr_file_);
    return FileReader::kAtEOF;
  }
  if (ferror(ptr_file_)) {
    DBGLOG("ERROR: ferror on read, GetLastError=" << GetLastError());
    return FileReader::kReadFailed;
  }
  return kSuccess;
}



uint64 FileReaderImpl::GetBytesAvailable() {
  uint64 available = 0;
  int64 offset = _ftelli64(ptr_file_);
  assert(offset == bytes_read_);
  int failed = _fseeki64(ptr_file_, 0, SEEK_END);
  if (failed) {
    DBGLOG("ERROR: could not seek to end of file, GetLastError="
           << GetLastError());
    return 0;
  }
  int64 file_size = _ftelli64(ptr_file_);
  if (file_size > bytes_read_) {
    available = file_size - bytes_read_;
  }
  failed = _fseeki64(ptr_file_, offset, SEEK_SET);
  if (failed) {
    DBGLOG("ERROR: could not seek to read pos, GetLastError="
           << GetLastError());
    return 0;
  }
  return available;
}

} // WebmLive
