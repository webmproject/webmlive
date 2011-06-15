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
#include "file_reader.h"
#include "http_uploader.h"

// curl error checking/logging macros
#define chkcurlform(X, Y) \
do { \
    if((X=(Y)) != CURL_FORMADD_OK) { \
      DBGLOG(#Y << " failed, val=" << X << "\n"); \
    } \
} while (0)

namespace WebmLive {

static const char* kContentType = "video/webm";
static const char* kFormName = "webm_file";
static const int kUnknownFileSize = -1;

class HttpUploaderImpl {
public:
  HttpUploaderImpl();
  ~HttpUploaderImpl();
  int Init(HttpUploaderSettings* ptr_settings);
private:
  int Final();
  int SetupForm(const HttpUploaderSettings* const);
  int SetHeaders(HttpUploaderSettings* ptr_settings);
  static int ProgressCallback(void* ptr_this,
                              double, double, // we ignore download progress
                              double upload_total, double upload_current);
  static size_t ReadCallback(char *buffer, size_t size, size_t nitems,
                             void *ptr_this);
  boost::scoped_ptr<FileReader> file_;
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
  if (!ptr_settings) {
    DBGLOG("ERROR: null ptr_settings");
    return E_INVALIDARG;
  }
  settings_.local_file = ptr_settings->local_file;
  settings_.target_url = ptr_settings->target_url;
  settings_.form_variables = ptr_settings->form_variables;
  settings_.headers = ptr_settings->headers;
  ptr_uploader_.reset(new (std::nothrow) HttpUploaderImpl());
  if (!ptr_uploader_) {
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

int HttpUploaderImpl::Init(HttpUploaderSettings* settings)
{
  file_.reset(new (std::nothrow) FileReader());
  if (!file_) {
    DBGLOG("ERROR: can't construct FileReader.");
    return E_OUTOFMEMORY;
  }
  int err = file_->Init(settings->local_file);
  if (err) {
    DBGLOG("ERROR: FileReader Init failed err=" << err);
    return err;
  }
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
  // pass user form variables to libcurl
  err = SetupForm(settings);
  if (err) {
    DBGLOG("ERROR: unable to set form variables, err=" << err);
    return err;
  }
  return ERROR_SUCCESS;
}

int HttpUploaderImpl::Final()
{
  if (ptr_curl_) {
    curl_easy_cleanup(ptr_curl_);
    ptr_curl_ = NULL;
  }
  DBGLOG("");
  return ERROR_SUCCESS;
}

int HttpUploaderImpl::SetupForm(const HttpUploaderSettings* const p)
{
  CURLFORMcode err;
  typedef std::map<std::string, std::string> StringMap;
  StringMap::const_iterator var_iter = p->form_variables.begin();
  curl_httppost* ptr_form_items = NULL;
  curl_httppost* ptr_last_form_item = NULL;
  // add user form variables
  for (; var_iter != p->form_variables.end(); ++var_iter) {
    err = curl_formadd(&ptr_form_items, &ptr_last_form_item,
                       CURLFORM_COPYNAME, var_iter->first.c_str(),
                       CURLFORM_COPYCONTENTS, var_iter->second.c_str(),
                       CURLFORM_END);
    if (err != CURL_FORMADD_OK) {
      DBGLOG("ERROR: curl_formadd failed err=" << err);
      return E_FAIL;
    }
  }
  // add file data
  err = curl_formadd(&ptr_form_items, &ptr_last_form_item,
                     CURLFORM_COPYNAME, kFormName,
                     // note that |CURLFORM_STREAM| relies on the callback
                     // set in the call to curl_easy_setopt with
                     // |CURLOPT_READFUNCTION| specified
                     CURLFORM_STREAM, reinterpret_cast<void*>(this),
                     CURLFORM_FILENAME, p->local_file.c_str(),
                     CURLFORM_CONTENTSLENGTH, kUnknownFileSize,
                     CURLFORM_CONTENTTYPE, kContentType,
                     CURLFORM_END);
  if (err != CURL_FORMADD_OK) {
    DBGLOG("ERROR: curl_formadd CURLFORM_FILE failed err=" << err);
    return E_FAIL;
  }
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
  HttpUploaderImpl* ptr_uploader_ =
    reinterpret_cast<HttpUploaderImpl*>(ptr_this);
  uint64 available = ptr_uploader_->file_->GetBytesAvailable();
  size_t requested = size * nitems;
  if (requested > available && available > 0) {
    requested = static_cast<size_t>(available);
  }
  size_t bytes_read = 0;
  if (available > 0) {
    int err = ptr_uploader_->file_->Read(requested, buffer, &bytes_read);
    if (err) {
      DBGLOG("FileReader out of data!");
    }
  }
  return bytes_read;
}

} // WebmLive
