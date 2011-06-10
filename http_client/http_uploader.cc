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

class HttpUploaderImpl {
public:
  HttpUploaderImpl();
  ~HttpUploaderImpl();
  int Init(HttpUploaderSettings*);
private:
  int Final();
  static int ProgressCallback(void* ptr_this,
                              double, double, // we ignore download progress
                              double upload_total, double upload_current);
  static size_t ReadCallback(char *buffer, size_t size, size_t nitems,
                             void *ptr_this);
  CURL* ptr_curl_;
  DISALLOW_COPY_AND_ASSIGN(HttpUploaderImpl);
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
  ptr_uploader_.reset(new (std::nothrow) HttpUploaderImpl());
  if (!ptr_uploader_)
  {
    DBGLOG("ERROR: can't construct HttpUploaderImpl.");
    return E_OUTOFMEMORY;
  }
  return ptr_uploader_->Init(&settings_);
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

HttpUploaderImpl::HttpUploaderImpl() :
  ptr_curl_(NULL)
{
  DBGLOG("");
}

HttpUploaderImpl::~HttpUploaderImpl()
{
  Final();
  DBGLOG("");
}

int HttpUploaderImpl::Init(HttpUploaderSettings*)
{
  // init libcurl
  ptr_curl_ = curl_easy_init();
  if (!ptr_curl_)
  {
    DBGLOG("curl_easy_init failed!");
    return E_FAIL;
  }
  CURLcode curl_ret = curl_easy_setopt(ptr_curl_, CURLOPT_NOPROGRESS, FALSE);
  if (curl_ret != CURLE_OK) {
    DBGLOG("ERROR: curl progress enable failed. curl_ret=" << curl_ret <<
      ":" << curl_easy_strerror(curl_ret));
    return E_FAIL;
  }
  // set the progress callback function pointer
  curl_ret = curl_easy_setopt(ptr_curl_, CURLOPT_PROGRESSFUNCTION,
                              ProgressCallback);
  if (curl_ret != CURLE_OK) {
    DBGLOG("ERROR: curl progress callback setup failed." << curl_ret <<
           ":" << curl_easy_strerror(curl_ret));
    return E_FAIL;
  }
  // set progress callback data pointer
  curl_ret = curl_easy_setopt(ptr_curl_, CURLOPT_PROGRESSDATA,
                              reinterpret_cast<void*>(this));
  if (curl_ret != CURLE_OK) {
    DBGLOG("ERROR: curl progress callback data setup failed." << curl_ret <<
           ":" << curl_easy_strerror(curl_ret));
    return E_FAIL;
  }
  // enable upload mode
  curl_ret = curl_easy_setopt(ptr_curl_, CURLOPT_UPLOAD, TRUE);
  if (curl_ret != CURLE_OK) {
    DBGLOG("ERROR: curl upload enable failed." << curl_ret <<
           ":" << curl_easy_strerror(curl_ret));
    return E_FAIL;
  }
  // set read callback function pointer
  curl_ret = curl_easy_setopt(ptr_curl_, CURLOPT_READFUNCTION, ReadCallback);
  if (curl_ret != CURLE_OK) {
    DBGLOG("ERROR: curl read callback setup failed." << curl_ret <<
           ":" << curl_easy_strerror(curl_ret));
    return E_FAIL;
  }
  // set read callback data pointer
  curl_ret = curl_easy_setopt(ptr_curl_, CURLOPT_READDATA,
                              reinterpret_cast<void*>(this));
  if (curl_ret != CURLE_OK) {
    DBGLOG("ERROR: curl read callback data setup failed." << curl_ret <<
           ":" << curl_easy_strerror(curl_ret));
    return E_FAIL;
  }
  // set our progress callback
  return ERROR_SUCCESS;
}

int HttpUploaderImpl::Final()
{
  if (ptr_curl_)
  {
    curl_easy_cleanup(ptr_curl_);
    ptr_curl_ = NULL;
  }
  DBGLOG("");
  return ERROR_SUCCESS;
}

int HttpUploaderImpl::ProgressCallback(void* ptr_this,
                                       double,
                                       double, // we ignore download progress
                                       double upload_total,
                                       double upload_current)
{
  DBGLOG("total=" << int(upload_total) << " current=" << int(upload_current));
  HttpUploaderImpl* ptr_uploader_ =
    reinterpret_cast<HttpUploaderImpl*>(ptr_this);
  ptr_uploader_;
  return 0;
}

size_t HttpUploaderImpl::ReadCallback(char *buffer, size_t size, size_t nitems,
                                      void *ptr_this)
{
  DBGLOG("size=" << size << " nitems=" << nitems);
  buffer;
  HttpUploaderImpl* ptr_uploader_ =
    reinterpret_cast<HttpUploaderImpl*>(ptr_this);
  ptr_uploader_;
  return 0;
}
