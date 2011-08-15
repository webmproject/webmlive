// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "win/file_reader_win.h"

#include <share.h>  // defines _SH_DENYNO, used with _wfsopen

#include <sstream>

#include "debug_util.h"
#include "file_util.h"

namespace webmlive {

FileReaderImpl::FileReaderImpl() : bytes_read_(0), ptr_file_(NULL) {
}

FileReaderImpl::~FileReaderImpl() {
  fclose(ptr_file_);
}

// Confirm |file_path| string is non-empty and call |Open|.
int FileReaderImpl::CreateFile(std::wstring file_path) {
  if (file_path.empty()) {
    DBGLOG("ERROR: empty file_path");
    return kInvalidArg;
  }
  file_path_ = file_path;
  if (Open(kModeCreate)) {
    DBGLOG("ERROR: could not open file, GetLastError=" << GetLastError());
    return kOpenFailed;
  }
  return kSuccess;
}

// Reopens the file via call to |Open| with |mode| set to |kModeOpen|, then
// calls |ReadFromStream| if |GetBytesAvailable| reports that data is available.
int FileReaderImpl::Read(size_t num_bytes, uint8* ptr_buffer,
                         size_t* ptr_num_read) {
  if (num_bytes < 1 || !ptr_buffer || !ptr_num_read) {
    return kInvalidArg;
  }
  size_t& num_read = *ptr_num_read;
  int status = Open(kModeOpen);
  if (status) {
    DBGLOG("ERROR: could not open file, GetLastError=" << GetLastError());
    return kOpenFailed;
  }
  if (GetBytesAvailable() > 0) {
    int err = ReadFromStream(num_bytes, ptr_buffer, num_read);
    if (err && err != FileReader::kAtEOF) {
      DBGLOG("ERROR: could not read file, err=" << err);
      return err;
    }
    bytes_read_ += num_read;
    if (num_bytes != num_read) {
      //DBGLOG("shortfall! requested=" << num_bytes << " read=" << num_read);
      return kAtEOF;
    }
  }
  return kSuccess;
}

// Uses |_wfsopen| to create or open file specified by |file_path_|.  Creates
// when |mode| is |kModeCreate|, opens when |mode| is |kModeOpen|. Seeks
// |ptr_file_| to |bytes_read_| if |bytes_read_| is non-zero.
int FileReaderImpl::Open(int mode) {
  const wchar_t* ptr_mode = NULL;
  if (mode == kModeCreate) {
    ptr_mode = L"w+b";
  } else if (mode == kModeOpen) {
    ptr_mode = L"r+b";
  }
  if (!ptr_mode) {
    DBGLOG("invalid mode specified");
    return kInvalidArg;
  }
  if (ptr_file_) {
    //DBGLOG("closing");
    fclose(ptr_file_);
    ptr_file_ = NULL;
  }
  //DBGLOG("opening, mode=" << ptr_mode);
  ptr_file_ = _wfsopen(file_path_.c_str(), ptr_mode, _SH_DENYNO);
  if (!ptr_file_) {
    DBGLOG("ERROR: could not open file, GetLastError=" << GetLastError());
    return kOpenFailed;
  }
  if (bytes_read_ > 0) {
    if (_fseeki64(ptr_file_, bytes_read_, SEEK_SET)) {
      DBGLOG("ERROR: could not seek to read pos, GetLastError="
             << GetLastError());
      DBGLOG("closing");
      fclose(ptr_file_);
      ptr_file_ = NULL;
      return kSeekFailed;
    }
  }
  return kSuccess;
}

// Calls |fread| and checks for errors using |feof| and |ferror|.  On end of
// file (feof returns true) |clearerr| is called to clear errors, and |kAtEOF|
// is returned. On failure reported by |fread|, |kReadError| is returned and
// |clearerr| is not called.
int FileReaderImpl::ReadFromStream(size_t num_bytes, uint8* ptr_buffer,
                                   size_t& num_read)
{
  num_read = fread(ptr_buffer, sizeof(uint8), num_bytes, ptr_file_);
  if (feof(ptr_file_)) {
    clearerr(ptr_file_);
    return kAtEOF;
  }
  if (ferror(ptr_file_)) {
    DBGLOG("ERROR: ferror on read, GetLastError=" << GetLastError());
    return kReadFailed;
  }
  return kSuccess;
}

// Seeks the |ptr_file_| to end of file to obtain total file size, and then
// subtracts |bytes_read_| to obtain number of bytes available for reading in
// |ptr_file_|.
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

}  // namespace webmlive
