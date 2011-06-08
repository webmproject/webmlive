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
#include "boost/shared_ptr.hpp"
#include "boost/thread/thread.hpp"
#include "chromium/base/basictypes.h"

#include <string>

struct HttpUploaderSettings {
  std::string local_file;
  std::string target_url;
};

class HttpUploaderImpl;

class HttpUploader {
public:
  HttpUploader();
  ~HttpUploader();
  int Init(HttpUploaderSettings* ptr_settings);
  void Go();
  void Stop();

private:
  void UploadThread();
  volatile bool stop_;
  HttpUploaderSettings settings_;
  boost::scoped_ptr<HttpUploaderImpl> ptr_uploader_;
  boost::shared_ptr<boost::thread> upload_thread_;
  DISALLOW_COPY_AND_ASSIGN(HttpUploader);
};

#endif // WEBMLIVE_HTTP_UPLOADER_H
