// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "http_client_base.h"
#include "file_util.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <shlwapi.h>

bool WebmLive::FileUtil::file_exists(std::wstring path)
{
  return PathFileExists(path.c_str()) == TRUE;
}

uint64 WebmLive::FileUtil::get_file_size(std::wstring path)
{
  uint64 file_size = 0;
#if 0
  WIN32_FILE_ATTRIBUTE_DATA attr = {0};
  if (GetFileAttributesEx(path.c_str(), GetFileExInfoStandard, &attr)) {
    file_size = (attr.nFileSizeHigh << 4) + attr.nFileSizeLow;
  }
#else
  struct _stat statbuf = {0};
  int result = _wstat(path.c_str(), &statbuf);
  if (result == NO_ERROR) {
    file_size = statbuf.st_size;
  }
#endif
  return file_size;
}
