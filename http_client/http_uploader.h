// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef WEBMLIVE_HTTP_UPLOADER_H
#define WEBMLIVE_HTTP_UPLOADER_H

#pragma once

#include "boost/scoped_ptr.hpp"
#include "chromium/base/basictypes.h"

#include <map>
#include <string>

namespace WebmLive {

struct HttpUploaderSettings {
  // |local_file| is what the HTTP server sees as the local file name.
  // Assigning a path to a local file and passing the settings struct to
  // |HttpUploader::Init| will not upload an existing file.
  std::string local_file;
  std::string target_url;
  typedef std::map<std::string, std::string> StringMap;
  StringMap form_variables;
  StringMap headers;
};

struct HttpUploaderStats {
  double bytes_per_second;
  int64 bytes_sent;
};

class HttpUploaderImpl;

class HttpUploader {
 public:
  enum {
    kUrlConfigError = -307,
    kFileReaderError = -306,
    kHeaderError = -305,
    kFormError = -304,
    kInvalidArg = -303,
    kInitFailed = -302,
    kRunFailed = -301,
    kSuccess = 0,
    kUploadInProgress = 1,
  };
  HttpUploader();
  ~HttpUploader();
  bool UploadComplete();
  int Init(HttpUploaderSettings* ptr_settings);
  int GetStats(HttpUploaderStats* ptr_stats);
  int Run();
  int Stop();
  int UploadBuffer(const uint8* const ptr_buffer, int32 length);
  // TODO(tomfinegan): Add UploadFile for upload of existing files. This will
  //                   complicate the upload thread, but will be worth it when
  //                   upload of existing files is implemented.
 private:
  HttpUploaderSettings settings_;
  boost::scoped_ptr<HttpUploaderImpl> ptr_uploader_;
  DISALLOW_COPY_AND_ASSIGN(HttpUploader);
};

} // WebmLive

#endif // WEBMLIVE_HTTP_UPLOADER_H
