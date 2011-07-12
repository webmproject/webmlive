// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "http_client_base.h"

#include <time.h>

#include "boost/shared_ptr.hpp"
#include "boost/thread/condition.hpp"
#include "boost/thread/thread.hpp"
#include "curl/curl.h"
#include "curl/types.h"
#include "curl/easy.h"
#include "debug_util.h"
#include "http_uploader.h"

#define LOG_CURL_ERR(CURL_ERR, MSG_STR) \
  DBGLOG("ERROR: " << MSG_STR << " err=" << CURL_ERR << ":" << \
         curl_easy_strerror(CURL_ERR))
#define LOG_CURLFORM_ERR(CURL_ERR, MSG_STR) \
  DBGLOG("ERROR: " << MSG_STR << " err=" << CURL_ERR)

namespace WebmLive {

static const char* kContentType = "video/webm";
static const char* kFormName = "webm_file";
static const char* kFileName = "live.webm";
static const int kUnknownFileSize = -1;
static const int kBytesRequiredForResume = 32*1024;

class HttpUploaderImpl {
 public:
  enum {
    kLibCurlError = -401,
    kSuccess = 0,
    kWriteCallbackStopRequest = 0,
    kProgressCallbackStopRequest = 1,
    kReadCallbackPauseRequest = CURL_READFUNC_PAUSE,
    kReadCallbackStopRequest = CURL_READFUNC_ABORT,
  };
  HttpUploaderImpl();
  ~HttpUploaderImpl();
  int Init(HttpUploaderSettings* ptr_settings);
  int UploadBuffer(const uint8* const ptr_buffer, int32 length);
  int GetStats(HttpUploaderStats* ptr_stats);
  int Run();
  int Stop();
 private:
  bool StopRequested();
  CURLcode SetCurlCallbacks();
  CURLcode SetHeaders(const HttpUploaderSettings* const ptr_settings);
  int Final();
  int SetUploadBuffer(const uint8* const ptr_buffer, int32 length);
  int SetUserFormVariables(const HttpUploaderSettings* const ptr_settings);
  static int ProgressCallback(void* ptr_this,
                              double, double, // we ignore download progress
                              double upload_total, double upload_current);
  static size_t WriteCallback(char* buffer, size_t size, size_t nitems,
                              void* ptr_this);
  void ResetStats();
  void UploadThread();
  bool stop_;
  boost::condition_variable buffer_ready_;
  boost::mutex mutex_;
  boost::shared_ptr<boost::thread> upload_thread_;
  clock_t start_ticks_;
  CURL* ptr_curl_;
  curl_httppost* ptr_form_;
  curl_httppost* ptr_form_end_;
  curl_slist* ptr_headers_;
  HttpUploaderStats stats_;
  DISALLOW_COPY_AND_ASSIGN(HttpUploaderImpl);
};

HttpUploader::HttpUploader()
{
}

HttpUploader::~HttpUploader()
{
}

// Copy user settings, and setup the internal uploader object
int HttpUploader::Init(HttpUploaderSettings* ptr_settings)
{
  if (!ptr_settings) {
    DBGLOG("ERROR: null ptr_settings");
    return kInvalidArg;
  }
  settings_.local_file = ptr_settings->local_file;
  settings_.target_url = ptr_settings->target_url;
  settings_.form_variables = ptr_settings->form_variables;
  settings_.headers = ptr_settings->headers;
  ptr_uploader_.reset(new (std::nothrow) HttpUploaderImpl());
  if (!ptr_uploader_) {
    DBGLOG("ERROR: can't construct HttpUploaderImpl.");
    return kInitFailed;
  }
  int status = ptr_uploader_->Init(&settings_);
  if (status) {
    DBGLOG("ERROR: uploader init failed. " << status);
    return kInitFailed;
  }
  return kSuccess;
}

int HttpUploader::GetStats(WebmLive::HttpUploaderStats* ptr_stats)
{
  return ptr_uploader_->GetStats(ptr_stats);
}

// Public method for kicking off the upload
int HttpUploader::Run()
{
  return ptr_uploader_->Run();
}

// Upload cancel method
int HttpUploader::Stop()
{
  return ptr_uploader_->Stop();
}

HttpUploaderImpl::HttpUploaderImpl()
    : ptr_curl_(NULL),
      ptr_form_(NULL),
      ptr_form_end_(NULL),
      ptr_headers_(NULL),
      stop_(false)
{
}

HttpUploaderImpl::~HttpUploaderImpl()
{
  Final();
}

// Initialize the upload
// - set basic libcurl settings (progress, read, and write callbacks)
// - call SetupForm to prepare for form/multipart upload, and pass user vars
// - call SetHeaders to pass user headers
int HttpUploaderImpl::Init(HttpUploaderSettings* settings)
{
  // init libcurl
  ptr_curl_ = curl_easy_init();
  if (!ptr_curl_)
  {
    DBGLOG("curl_easy_init failed!");
    return kLibCurlError;
  }
  CURLcode curl_ret = curl_easy_setopt(ptr_curl_, CURLOPT_URL,
                                       settings->target_url.c_str());
  if (curl_ret != CURLE_OK) {
    LOG_CURL_ERR(curl_ret, "could not pass URL to curl.");
    return HttpUploader::kUrlConfigError;
  }
  // enable progress reports
  curl_ret = curl_easy_setopt(ptr_curl_, CURLOPT_NOPROGRESS, FALSE);
  if (curl_ret != CURLE_OK) {
    LOG_CURL_ERR(curl_ret, "curl progress enable failed.");
    return kLibCurlError;
  }
  // set callbacks
  curl_ret = SetCurlCallbacks();
  if (curl_ret != CURLE_OK) {
    LOG_CURL_ERR(curl_ret, "curl callback setup failed.");
    return kLibCurlError;
  }
  int err = SetUserFormVariables(settings);
  if (err) {
    DBGLOG("unable to set user form variables, err=" << err);
    return HttpUploader::kFormError;
  }
  curl_ret = SetHeaders(settings);
  if (curl_ret) {
    LOG_CURL_ERR(curl_ret, "unable to set headers.");
    return HttpUploader::kHeaderError;
  }
  ResetStats();
  return kSuccess;
}

int HttpUploaderImpl::UploadBuffer(const uint8* const ptr_buf, int32 length)
{
  if (SetUploadBuffer(ptr_buf, length)) {
    DBGLOG("ERROR: SetUploadBuffer failed!");
    return HttpUploader::kRunFailed;
  }
  CURLcode err = curl_easy_perform(ptr_curl_);
  if (err != CURLE_OK) {
    LOG_CURL_ERR(err, "curl_easy_perform failed.");
  } else {
    int resp_code = 0;
    curl_easy_getinfo(ptr_curl_, CURLINFO_RESPONSE_CODE, &resp_code);
    DBGLOG("server response code: " << resp_code);
  }
  return err;
}

int HttpUploaderImpl::GetStats(HttpUploaderStats* ptr_stats)
{
  if (!ptr_stats) {
    DBGLOG("ERROR: NULL ptr_stats");
    return HttpUploader::kInvalidArg;
  }
  boost::mutex::scoped_lock lock(mutex_);
  ptr_stats->bytes_per_second = stats_.bytes_per_second;
  ptr_stats->bytes_sent = stats_.bytes_sent;
  return kSuccess;
}

int HttpUploaderImpl::Run()
{
  assert(!upload_thread_);
  using boost::bind;
  using boost::shared_ptr;
  using boost::thread;
  using std::nothrow;
  upload_thread_ = shared_ptr<thread>(
    new (nothrow) thread(bind(&HttpUploaderImpl::UploadThread, this)));
  return kSuccess;
}

int HttpUploaderImpl::Stop()
{
  assert(upload_thread_);
  boost::mutex::scoped_lock lock(mutex_);
  stop_ = true;
  lock.unlock();
  upload_thread_->join();
  return kSuccess;
}

bool HttpUploaderImpl::StopRequested()
{
  bool stop_requested = false;
  boost::mutex::scoped_try_lock lock(mutex_);
  if (lock.owns_lock()) {
    stop_requested = stop_;
  }
  return stop_requested;
}

CURLcode HttpUploaderImpl::SetCurlCallbacks()
{
  // set the progress callback function pointer
  CURLcode err = curl_easy_setopt(ptr_curl_, CURLOPT_PROGRESSFUNCTION,
                                  ProgressCallback);
  if (err != CURLE_OK) {
    LOG_CURL_ERR(err, "curl progress callback setup failed.");
    return err;
  }
  // set progress callback data pointer
  err = curl_easy_setopt(ptr_curl_, CURLOPT_PROGRESSDATA,
                         reinterpret_cast<void*>(this));
  if (err != CURLE_OK) {
    LOG_CURL_ERR(err, "curl progress callback data setup failed.");
    return err;
  }
  // set write callback function pointer
  err = curl_easy_setopt(ptr_curl_, CURLOPT_WRITEFUNCTION, WriteCallback);
  if (err != CURLE_OK) {
    LOG_CURL_ERR(err, "curl write callback setup failed.");
    return err;
  }
  // set write callback data pointer
  err = curl_easy_setopt(ptr_curl_, CURLOPT_WRITEDATA,
                         reinterpret_cast<void*>(this));
  if (err != CURLE_OK) {
    LOG_CURL_ERR(err, "curl write callback data setup failed.");
    return err;
  }
  return err;
}

// Disable HTTP 100 responses (send empty Expect header), and pass user HTTP
// headers into lib curl.
CURLcode HttpUploaderImpl::SetHeaders(const HttpUploaderSettings* const p)
{
  // Disable HTTP 100 with an empty Expect header
  std::string expect_header = "Expect:";
  ptr_headers_ = curl_slist_append(ptr_headers_, expect_header.c_str());
  typedef std::map<std::string, std::string> StringMap;
  const HttpUploaderSettings& settings = *p;
  StringMap::const_iterator header_iter = settings.headers.begin();
  // add user headers
  for (; header_iter != settings.headers.end(); ++header_iter) {
    std::ostringstream header;
    header << header_iter->first.c_str() << ":" << header_iter->second.c_str();
    ptr_headers_ = curl_slist_append(ptr_headers_, header.str().c_str());
  }
  CURLcode err = curl_easy_setopt(ptr_curl_, CURLOPT_HTTPHEADER, ptr_headers_);
  if (err != CURLE_OK) {
    LOG_CURL_ERR(err, "setopt CURLOPT_HTTPHEADER failed err=");
  }
  return err;
}

// HttpUploaderImpl cleanup function
int HttpUploaderImpl::Final()
{
  if (ptr_curl_) {
    curl_easy_cleanup(ptr_curl_);
    ptr_curl_ = NULL;
  }
  if (ptr_form_) {
    curl_formfree(ptr_form_);
    ptr_form_ = NULL;
  }
  if (ptr_headers_) {
    curl_slist_free_all(ptr_headers_);
    ptr_headers_ = NULL;
  }
  DBGLOG("");
  return kSuccess;
}

int HttpUploaderImpl::SetUploadBuffer(const uint8* const ptr_buffer,
                                      int32 length)
{
  // Pass the buffer pointer to libcurl
  CURLFORMcode err = curl_formadd(&ptr_form_, &ptr_form_end_,
                                  CURLFORM_COPYNAME, kFormName,
                                  CURLFORM_BUFFER, kFileName,
                                  CURLFORM_BUFFERPTR, ptr_buffer,
                                  CURLFORM_BUFFERLENGTH, length,
                                  CURLFORM_CONTENTTYPE, kContentType,
                                  CURLFORM_END);
  if (err != CURL_FORMADD_OK) {
    LOG_CURLFORM_ERR(err, "curl_formadd CURLFORM_FILE failed.");
    return err;
  }
  CURLcode err_setopt = curl_easy_setopt(ptr_curl_, CURLOPT_HTTPPOST,
                                         ptr_form_);
  if (err_setopt != CURLE_OK) {
    LOG_CURL_ERR(err_setopt, "setopt CURLOPT_HTTPPOST failed.");
    return err;
  }
  return kSuccess;
}

// Set necessary curl options for form file upload, and add the user form
// variables.  Note the use of |CURLFORM_STREAM|, it completes the read
// callback setup.
int HttpUploaderImpl::SetUserFormVariables(const HttpUploaderSettings* const p)
{
  typedef std::map<std::string, std::string> StringMap;
  const HttpUploaderSettings& settings = *p;
  StringMap::const_iterator var_iter = settings.form_variables.begin();
  CURLFORMcode err;
  // add user form variables
  for (; var_iter != settings.form_variables.end(); ++var_iter) {
    err = curl_formadd(&ptr_form_, &ptr_form_end_,
                       CURLFORM_COPYNAME, var_iter->first.c_str(),
                       CURLFORM_COPYCONTENTS, var_iter->second.c_str(),
                       CURLFORM_END);
    if (err != CURL_FORMADD_OK) {
      LOG_CURLFORM_ERR(err, "curl_formadd failed.");
      return HttpUploader::kFormError;
    }
  }
  CURLcode err_setopt = curl_easy_setopt(ptr_curl_, CURLOPT_HTTPPOST,
                                         ptr_form_);
  if (err_setopt != CURLE_OK) {
    LOG_CURL_ERR(err_setopt, "setopt CURLOPT_HTTPPOST failed.");
    return err_setopt;
  }
  return kSuccess;
}

// Handle libcurl progress updates
int HttpUploaderImpl::ProgressCallback(void* ptr_this,
                                       double download_total,
                                       double download_current,
                                       double upload_total,
                                       double upload_current)
{
  download_total; download_current;   // we ignore download progress
  upload_total; // we use |upload_total| only in DBGLOGs
  HttpUploaderImpl* ptr_uploader_ =
    reinterpret_cast<HttpUploaderImpl*>(ptr_this);
  if (ptr_uploader_->StopRequested()) {
    DBGLOG("stop requested.");
    return kProgressCallbackStopRequest;
  }
  boost::mutex::scoped_lock lock(ptr_uploader_->mutex_);
  HttpUploaderStats& stats = ptr_uploader_->stats_;
  stats.bytes_sent = static_cast<int64>(upload_current);
  double ticks_elapsed = clock() - ptr_uploader_->start_ticks_;
  double ticks_per_sec = CLOCKS_PER_SEC;
  stats.bytes_per_second = upload_current / (ticks_elapsed / ticks_per_sec);
  DBGLOG("total=" << int(upload_total) << " bytes_per_sec="
         << int(stats.bytes_per_second));
  return 0;
}

// Handle HTTP response data
size_t HttpUploaderImpl::WriteCallback(char* buffer, size_t size, size_t nitems,
                                       void* ptr_this)
{
  //DBGLOG("size=" << size << " nitems=" << nitems);
  // TODO(tomfinegan): store response data for users
  std::string tmp;
  tmp.assign(buffer, size*nitems);
  DBGLOG("from server: " << tmp.c_str());
  HttpUploaderImpl* ptr_uploader_ =
    reinterpret_cast<HttpUploaderImpl*>(ptr_this);
  if (ptr_uploader_->StopRequested()) {
    DBGLOG("stop requested.");
    return kWriteCallbackStopRequest;
  }
  return size*nitems;
}

// Reset uploaded byte count, and store upload start time
void HttpUploaderImpl::ResetStats()
{
  boost::mutex::scoped_lock lock(mutex_);
  stats_.bytes_per_second = 0;
  stats_.bytes_sent = 0;
  start_ticks_ = clock();
}

// Upload thread wrapper
void HttpUploaderImpl::UploadThread()
{
  DBGLOG("running...");
  // TODO(tomfinegan): monitor |buffer_ready|, and kick off uploads when we
  //                   have data!
  DBGLOG("thread done");
}

} // WebmLive
