// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "http_client_base.h"

#define CURL_STATICLIB
#include "curl/curl.h"
#include "curl/types.h"
#include "curl/easy.h"

#include "debug_util.h"
#include "http_uploader.h"

// curl error checking/logging macros
#define chkcurl(X, Y) \
do { \
    if((X=(Y)) != CURLE_OK) { \
        DBGLOG(#Y << " failed, error:\n  " << curl_easy_strerror(X) << "\n"); \
    } \
} while (0)

#define chkcurlform(X, Y) \
do { \
    if((X=(Y)) != CURL_FORMADD_OK) { \
      DBGLOG(#Y << " failed, val=" << X << "\n"); \
    } \
} while (0)

class CurlHttpUploader {
public:
  CurlHttpUploader();
  ~CurlHttpUploader();
  int Init(HttpUploaderSettings*);
  int Final();
private:
  CURL* ptr_curl_;
};

typedef CurlHttpUploader HttpUploaderImpl;

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

CurlHttpUploader::CurlHttpUploader() :
  ptr_curl_(NULL)
{
}

CurlHttpUploader::~CurlHttpUploader()
{
  Final();
}

int CurlHttpUploader::Init(HttpUploaderSettings*)
{
  Final();
  ptr_curl_ = curl_easy_init();
  if (!ptr_curl_)
  {
    DBGLOG("curl_easy_init failed!");
    return E_FAIL;
  }
  return ERROR_SUCCESS;
}

int CurlHttpUploader::Final()
{
  if (ptr_curl_)
  {
    curl_easy_cleanup(ptr_curl_);
    ptr_curl_ = NULL;
  }
  return ERROR_SUCCESS;
}
