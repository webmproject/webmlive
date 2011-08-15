// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include <shlwapi.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "file_util.h"
#include "http_client_base.h"

bool webmlive::FileUtil::file_exists(std::string path) {
  return PathFileExists(path.c_str()) == TRUE;
}

uint64 webmlive::FileUtil::get_file_size(std::string path) {
  uint64 file_size = 0;
  struct _stat statbuf = {0};
  int result = _stat(path.c_str(), &statbuf);
  if (result == NO_ERROR) {
    file_size = statbuf.st_size;
  }
  return file_size;
}
