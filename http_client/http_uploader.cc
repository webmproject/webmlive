// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "http_client_base.h"

#include "debug_util.h"
#include "http_uploader.h"

class HttpUploaderImpl {
};

HttpUploader::HttpUploader():
  stop_(false)
{
}

HttpUploader::~HttpUploader()
{
}

int HttpUploader::Init(HttpUploaderSettings* ptr_settings)
{
  if (!ptr_settings)
  {
    DBGLOG("ERROR: null ptr_settings");
    return E_INVALIDARG;
  }
  settings_.local_file = ptr_settings->local_file;
  settings_.target_url = ptr_settings->target_url;
  return S_OK;
}

void HttpUploader::Go()
{
  assert(!upload_thread_);
  upload_thread_ = boost::shared_ptr<boost::thread>(
    new boost::thread(boost::bind(&HttpUploader::UploadThread, this)));
}

void HttpUploader::Stop()
{
  assert(upload_thread_);
  stop_ = true;
  upload_thread_->join();
}

void HttpUploader::UploadThread()
{
  DBGLOG("running...");
  using boost::thread;
  while (stop_ == false) {
    printf(".");
    thread::yield();
  }
  DBGLOG("thread done");
}

