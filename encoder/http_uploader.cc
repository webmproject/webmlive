// Copyright (c) 2012 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "encoder/http_uploader.h"

#include <cassert>
#include <ctime>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include "encoder/buffer_util.h"
#include "curl/curl.h"
#include "curl/easy.h"
#include "glog/logging.h"
#include "libwebm/mkvparser.hpp"

#define LOG_CURL_ERR(CURL_ERR, MSG_STR) \
  LOG(ERROR) << MSG_STR << " err=" << CURL_ERR << ":" \
             << curl_easy_strerror(CURL_ERR)
#define LOG_CURLFORM_ERR(CURL_ERR, MSG_STR) \
  LOG(ERROR) << MSG_STR << " err=" << CURL_ERR

namespace webmlive {

static const char kExpectHeader[] = "Expect:";
static const char kContentTypeHeader[] = "Content-Type: video/webm";
static const char kFormName[] = "webm_file";
static const char kWebmMimeType[] = "video/webm";
static const char kContentIdHeader[] = "X-Content-Id: ";

class HttpUploaderImpl {
 public:
  HttpUploaderImpl();
  ~HttpUploaderImpl();

  // Copies user settings and configures libcurl.
  bool Init(const HttpUploaderSettings& settings);

  // Locks |mutex_| and copies current stats to |ptr_stats|.
  bool GetStats(HttpUploaderStats* ptr_stats);

  // Runs |UploadThread|, and starts waiting for user data.
  bool Run();

  // Uploads user data.
  bool UploadBuffer(const std::string& id,
                    const uint8* ptr_buffer, int32 length);

  // Stops the uploader.
  bool Stop();

 private:
  // Used by |UploadThread|. Returns true if user has called |Stop|.
  bool StopRequested();

  // Pass our callbacks, |ProgressCallback| and |WriteCallback|, to libcurl.
  CURLcode SetCurlCallbacks();

  // Pass user HTTP headers to libcurl, and disable HTTP 100 responses.
  CURLcode SetHeaders(const std::string& content_id);

  // Configures libcurl to POST data buffers as file data in a form/multipart
  // HTTP POST.
  bool SetupFormPost(const uint8* const ptr_buffer, int32 length);

  // Configures libcurl to POST data buffers as HTTP POST content-data.
  bool SetupPost(const uint8* const ptr_buffer, int32 length);

  // Upload user data with libcurl.
  bool Upload(BufferQueue::Buffer* buffer);

  // Wakes up |UploadThread| when users pass data through |UploadBuffer|.
  void WaitForUserData();

  // Libcurl progress callback function.  Acquires |mutex_| and updates
  // |stats_|.
  static int ProgressCallback(void* ptr_this,
                              double /*download_total*/,
                              double /*download_current*/,
                              double upload_total, double upload_current);

  // Logs HTTP response data received by libcurl.
  static size_t WriteCallback(char* buffer, size_t size, size_t nitems,
                              void* ptr_this);

  // Acquires |mutex_|, resets |stats_| and sets |start_ticks_|.
  void ResetStats();

  // Thread function. Wakes when |WaitForUserData| is notified by
  // |UploadBuffer|, and calls |Upload| to POST user data to the HTTP server
  // using libcurl.
  void UploadThread();

  // Frees HTTP header list.
  void FreeHeaders();

  // Stop flag. Internal callers use |StopRequested| to allow for
  // synchronization via |mutex_|.  Set by |Stop|, and responded to in
  // |UploadThread|.
  bool stop_;

  // Upload complete/ready to upload flag.  Initializes to true to allow
  // users of the uploader to base all Upload calls on |UploadComplete|.
  bool upload_complete_;

  // Condition variable used to wake |UploadThread| when a user code passes a
  // buffer to |UploadBuffer|.
  std::condition_variable wake_condition_;

  // Mutex for synchronization of public method calls with |UploadThread|
  // activity. Mutable so |UploadComplete()| can be a const method.
  mutable std::mutex mutex_;

  // Thread object.
  std::shared_ptr<std::thread> upload_thread_;

  // Uploader start time.  Reset when via |ResetStatts| when |Init| is called.
  clock_t start_ticks_;

  // Libcurl pointer.
  CURL* ptr_curl_;

  // Libcurl form variable/data chain.
  curl_httppost* ptr_form_;

  // Pointer to end of libcurl form chain.
  curl_httppost* ptr_form_end_;

  // Pointer to list of user HTTP headers.
  curl_slist* ptr_headers_;

  // Uploader settings.
  HttpUploaderSettings settings_;

  // Basic stats stored by |ProgressCallback|.
  HttpUploaderStats stats_;

  // Simple buffer object that remains locked while libcurl uploads data in
  // |Upload|.  This second locking mechanism is in place to allow |mutex_| to
  // be unlocked while uploads are in progress (which prevents public methods
  // from blocking).
  BufferQueue upload_buffer_;

  // The name of the file on the local system.  Note that it is not being read,
  // it's information included within the form data contained within the HTTP
  // post.
  std::string local_file_name_;

  WEBMLIVE_DISALLOW_COPY_AND_ASSIGN(HttpUploaderImpl);
};

///////////////////////////////////////////////////////////////////////////////
// HttpUploader
//

HttpUploader::HttpUploader() {
}

HttpUploader::~HttpUploader() {
}

// Copy user settings, and setup the internal uploader object.
bool HttpUploader::Init(const HttpUploaderSettings& settings) {
  ptr_uploader_.reset(new (std::nothrow) HttpUploaderImpl());  // NOLINT
  if (!ptr_uploader_) {
    LOG(ERROR) << "Out of memory.";
    return false;
  }
  int status = ptr_uploader_->Init(settings);
  if (status) {
    LOG(ERROR) << "uploader init failed. " << status;
    return false;
  }
  return true;
}

// Return result of |GetStats| on |ptr_uploader_|.
bool HttpUploader::GetStats(webmlive::HttpUploaderStats* ptr_stats) {
  return ptr_uploader_->GetStats(ptr_stats);
}

// Return result of |Run| on |ptr_uploader_|.
bool HttpUploader::Run() {
  return ptr_uploader_->Run();
}

// Return result of |Stop| on |ptr_uploader_|.
bool HttpUploader::Stop() {
  return ptr_uploader_->Stop();
}

// Return result of |UploadBuffer| on |ptr_uploader_|.
bool HttpUploader::UploadBuffer(const std::string& id,
                                const uint8* ptr_buffer, int length) {
  return ptr_uploader_->UploadBuffer(id, ptr_buffer, length);
}

///////////////////////////////////////////////////////////////////////////////
// HttpUploaderImpl
//

HttpUploaderImpl::HttpUploaderImpl()
    : ptr_curl_(NULL),
      ptr_form_(NULL),
      ptr_form_end_(NULL),
      ptr_headers_(NULL),
      stop_(false),
      upload_complete_(true) {
}

HttpUploaderImpl::~HttpUploaderImpl() {
  if (ptr_curl_) {
    curl_easy_cleanup(ptr_curl_);
    ptr_curl_ = NULL;
  }
  if (ptr_form_) {
    curl_formfree(ptr_form_);
    ptr_form_ = NULL;
    ptr_form_end_ = NULL;
  }
  FreeHeaders();
}

// Initializes the upload:
// - copies user settings
// - sets basic libcurl settings (progress and write callbacks)
bool HttpUploaderImpl::Init(const HttpUploaderSettings& settings) {
  if (settings.target_url.empty()) {
    LOG(ERROR) << "Empty target URL.";
    return false;
  }

  // copy user settings
  settings_ = settings;

  // Init libcurl.
  ptr_curl_ = curl_easy_init();
  if (!ptr_curl_) {
    LOG(ERROR) << "curl_easy_init failed!";
    return false;
  }

  // Enable progress reports from libcurl.
  CURLcode curl_ret = curl_easy_setopt(ptr_curl_, CURLOPT_NOPROGRESS, FALSE);
  if (curl_ret != CURLE_OK) {
    LOG_CURL_ERR(curl_ret, "curl progress enable failed.");
    return false;
  }

  // Set callbacks.
  curl_ret = SetCurlCallbacks();
  if (curl_ret != CURLE_OK) {
    LOG_CURL_ERR(curl_ret, "curl callback setup failed.");
    return false;
  }

  local_file_name_ = settings_.local_file;
  ResetStats();
  return false;
}

// Obtain lock on |mutex_| and copy current stats values from |stats_| to
// |ptr_stats|.
bool HttpUploaderImpl::GetStats(HttpUploaderStats* ptr_stats) {
  if (!ptr_stats) {
    LOG(ERROR) << "NULL ptr_stats";
    return false;
  }
  std::lock_guard<std::mutex> lock(mutex_);
  ptr_stats->bytes_per_second = stats_.bytes_per_second;
  ptr_stats->bytes_sent_current = stats_.bytes_sent_current;
  ptr_stats->total_bytes_uploaded = stats_.total_bytes_uploaded;
  return true;
}

// Run |UploadThread| using |std::thread|.
bool HttpUploaderImpl::Run() {
  assert(!upload_thread_);
  using std::bind;
  using std::shared_ptr;
  using std::thread;
  using std::nothrow;
  upload_thread_ = shared_ptr<thread>(
      new (nothrow) thread(bind(&HttpUploaderImpl::UploadThread,  // NOLINT
                                this)));
  if (!upload_thread_) {
    LOG(ERROR) << "Out of memory.";
    return false;
  }
  return true;
}

// Enqueue the user buffer. Does not lock |mutex_|; relies on
// |upload_buffer_|'s internal lock.
bool HttpUploaderImpl::UploadBuffer(const std::string& id,
                                    const uint8* ptr_buf, int length) {
  if (!upload_buffer_.EnqueueBuffer(id, ptr_buf, length)) {
    LOG(ERROR) << "Upload buffer enqueue failed.";
    return false;
  }
  // Wake |UploadThread|.
  LOG(INFO) << "waking uploader with " << length << " bytes";
  wake_condition_.notify_one();
  return true;
}

// Stops UploadThread() by obtaining lock on |mutex_| and setting |stop_| to
// true, and then waking the upload thread by calling notify_one() on
// |wake_condition_|.
// The lock on |mutex_| is released before calling notify_one() to ensure that
// a running upload stops when StopRequested() is called within the libcurl
// callbacks.
bool HttpUploaderImpl::Stop() {
  if (!upload_thread_) {
    LOG(ERROR) << "Upload thread not running!";
    return false;
  }
  mutex_.lock();
  stop_ = true;
  mutex_.unlock();

  // Wake up the upload thread.
  wake_condition_.notify_one();
  // And wait for it to exit.
  upload_thread_->join();
  return true;
}

// Try to obtain lock on |mutex_|, and return the value of |stop_| if lock is
// obtained.  Returns false if unable to obtain the lock.
bool HttpUploaderImpl::StopRequested() {
  bool stop_requested = false;
  std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
  if (lock.owns_lock()) {
    stop_requested = stop_;
  }
  return stop_requested;
}

// Pass callback function pointers (|ProgressCallback| and |WriteCallback|),
// and data, |this|, to libcurl.
CURLcode HttpUploaderImpl::SetCurlCallbacks() {
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
CURLcode HttpUploaderImpl::SetHeaders(const std::string& content_id) {
  FreeHeaders();
  // Tell libcurl to omit "Expect: 100-continue" from requests
  ptr_headers_ = curl_slist_append(ptr_headers_, kExpectHeader);
  if (settings_.post_mode == webmlive::HTTP_POST) {
    // In form posts the video/webm mime-type is included in the form itself,
    // but in plain old HTTP posts the Content-Type must be video/webm.
    ptr_headers_ = curl_slist_append(ptr_headers_, kContentTypeHeader);
  }
  typedef std::map<std::string, std::string> StringMap;
  StringMap::const_iterator header_iter = settings_.headers.begin();
  // add user headers
  for (; header_iter != settings_.headers.end(); ++header_iter) {
    std::ostringstream header;
    header << header_iter->first.c_str() << ":" << header_iter->second.c_str();
    ptr_headers_ = curl_slist_append(ptr_headers_, header.str().c_str());
  }
  // add |content_id|.
  const std::string content_id_header = kContentIdHeader + content_id;
  ptr_headers_ = curl_slist_append(ptr_headers_, content_id_header.c_str());
  const CURLcode err = curl_easy_setopt(ptr_curl_,
                                        CURLOPT_HTTPHEADER, ptr_headers_);
  if (err != CURLE_OK) {
    LOG_CURL_ERR(err, "setopt CURLOPT_HTTPHEADER failed err=");
  }
  return err;
}

// Sets necessary curl options for form based file upload, and adds the user
// form variables.
bool HttpUploaderImpl::SetupFormPost(const uint8* const ptr_buffer,
                                     int length) {
  if (ptr_form_) {
    curl_formfree(ptr_form_);
    ptr_form_ = NULL;
    ptr_form_end_ = NULL;
  }
  typedef std::map<std::string, std::string> StringMap;
  StringMap::const_iterator var_iter = settings_.form_variables.begin();
  CURLFORMcode err;
  // add user form variables
  for (; var_iter != settings_.form_variables.end(); ++var_iter) {
    err = curl_formadd(&ptr_form_, &ptr_form_end_,
                       CURLFORM_COPYNAME, var_iter->first.c_str(),
                       CURLFORM_COPYCONTENTS, var_iter->second.c_str(),
                       CURLFORM_END);
    if (err != CURL_FORMADD_OK) {
      LOG_CURLFORM_ERR(err, "curl_formadd failed.");
      return false;
    }
  }
  // add buffer to form
  err = curl_formadd(&ptr_form_, &ptr_form_end_,
                     CURLFORM_COPYNAME, kFormName,
                     CURLFORM_BUFFER, local_file_name_.c_str(),
                     CURLFORM_BUFFERPTR, ptr_buffer,
                     CURLFORM_BUFFERLENGTH, length,
                     CURLFORM_CONTENTTYPE, kWebmMimeType,
                     CURLFORM_END);
  if (err != CURL_FORMADD_OK) {
    LOG_CURLFORM_ERR(err, "curl_formadd CURLFORM_FILE failed.");
    return false;
  }
  // pass the form to libcurl
  CURLcode err_setopt = curl_easy_setopt(ptr_curl_, CURLOPT_HTTPPOST,
                                         ptr_form_);
  if (err_setopt != CURLE_OK) {
    LOG_CURL_ERR(err_setopt, "setopt CURLOPT_HTTPPOST failed.");
    return false;
  }
  return true;
}

// Configures libcurl to POST data buffers as HTTP POST content-data.
bool HttpUploaderImpl::SetupPost(const uint8* const ptr_buffer, int length) {
  CURLcode err_setopt = curl_easy_setopt(ptr_curl_, CURLOPT_POST, ptr_form_);
  if (err_setopt != CURLE_OK) {
    LOG_CURL_ERR(err_setopt, "setopt CURLOPT_HTTPPOST failed.");
    return false;
  }
  // Pass |ptr_buffer| to libcurl; it's used in the call to |curl_easy_perform|
  err_setopt = curl_easy_setopt(ptr_curl_, CURLOPT_POSTFIELDS, ptr_buffer);
  if (err_setopt != CURLE_OK) {
    LOG_CURL_ERR(err_setopt, "setopt CURLOPT_POSTFIELDS failed.");
    return false;
  }
  // Tell libcurl the size of |ptr_buffer|.  If libcurl is not informed of the
  // size before the call to |curl_easy_perform|, it will use strlen to
  // determine the length of the data.
  err_setopt = curl_easy_setopt(ptr_curl_, CURLOPT_POSTFIELDSIZE, length);
  if (err_setopt != CURLE_OK) {
    LOG_CURL_ERR(err_setopt, "setopt CURLOPT_POSTFIELDSIZE failed.");
    return false;
  }
  return true;
}

// Upload data using libcurl.
bool HttpUploaderImpl::Upload(BufferQueue::Buffer* buffer) {
  LOG(INFO) << "upload buffer size=" << buffer->data.size();
  CURLcode err = curl_easy_setopt(ptr_curl_, CURLOPT_URL,
                                  settings_.target_url.c_str());
  if (err != CURLE_OK) {
    LOG_CURL_ERR(err, "could not pass URL to curl.");
    return false;
  }
  if (settings_.post_mode == webmlive::HTTP_FORM_POST) {
    if (!SetupFormPost(&buffer->data[0], buffer->data.size())) {
      LOG(ERROR) << "SetupFormPost failed!";
      return false;
    }
  } else {
    if (!SetupPost(&buffer->data[0], buffer->data.size())) {
      LOG(ERROR) << "SetupPost failed!";
      return false;
    }
  }

  // Disable HTTP 100 responses, and set user HTTP headers.
  err = SetHeaders(buffer->id);
  if (err) {
    LOG_CURL_ERR(err, "unable to set headers.");
    return false;
  }
  err = curl_easy_perform(ptr_curl_);
  if (err != CURLE_OK) {
    LOG_CURL_ERR(err, "curl_easy_perform failed.");
  } else {
    int resp_code = 0;
    curl_easy_getinfo(ptr_curl_, CURLINFO_RESPONSE_CODE, &resp_code);
    LOG(INFO) << "server response code: " << resp_code;
  }

  // Update total bytes uploaded.
  double bytes_uploaded = 0;
  err = curl_easy_getinfo(ptr_curl_, CURLINFO_SIZE_UPLOAD, &bytes_uploaded);
  if (err != CURLE_OK) {
    LOG_CURL_ERR(err, "curl_easy_getinfo CURLINFO_SIZE_UPLOAD failed.");
  } else {
    std::lock_guard<std::mutex> lock(mutex_);
    stats_.bytes_sent_current = 0;
    stats_.total_bytes_uploaded += static_cast<int64>(bytes_uploaded);
  }
  VLOG(1) << "upload complete.";
  return true;
}

// Idle the upload thread while awaiting user data.
void HttpUploaderImpl::WaitForUserData() {
  std::unique_lock<std::mutex> lock(mutex_);
  wake_condition_.wait(lock);  // Unlock |mutex_| and idle the thread while we
                               // wait for the next chunk of user data.
}

// Handle libcurl progress updates. Returns 1 to signal that libcurl should stop
// sending data. Returns 0 otherwise.
int HttpUploaderImpl::ProgressCallback(void* ptr_this,
                                       double /*download_total*/,
                                       double /*download_current*/,
                                       double upload_total,
                                       double upload_current) {
  HttpUploaderImpl* ptr_uploader_ =
      reinterpret_cast<HttpUploaderImpl*>(ptr_this);
  if (ptr_uploader_->StopRequested()) {
    LOG(ERROR) << "stop requested.";
    return 1;
  }
  std::lock_guard<std::mutex> lock(ptr_uploader_->mutex_);
  HttpUploaderStats& stats = ptr_uploader_->stats_;
  stats.bytes_sent_current = static_cast<int64>(upload_current);
  double ticks_elapsed = clock() - ptr_uploader_->start_ticks_;
  double ticks_per_sec = CLOCKS_PER_SEC;
  stats.bytes_per_second =
      (upload_current + stats.total_bytes_uploaded) /
      (ticks_elapsed / ticks_per_sec);
  VLOG(4) << "total=" << static_cast<int>(upload_total) << " bytes_per_sec="
          << static_cast<int>(stats.bytes_per_second);
  return CURLE_OK;
}

// Handle HTTP response data.
size_t HttpUploaderImpl::WriteCallback(char* buffer, size_t size,
                                       size_t nitems,
                                       void* ptr_this) {
  VLOG(4) << "size=" << size << " nitems=" << nitems;
  // TODO(tomfinegan): store response data for users
  std::string tmp;
  tmp.assign(buffer, size*nitems);
  LOG(INFO) << "from server:\n" << tmp.c_str();
  HttpUploaderImpl* ptr_uploader_ =
    reinterpret_cast<HttpUploaderImpl*>(ptr_this);
  if (ptr_uploader_->StopRequested()) {
    LOG(INFO) << "stop requested.";
    return 0;
  }
  return size*nitems;
}

// Reset uploaded byte count, and store upload start time.
void HttpUploaderImpl::ResetStats() {
  std::lock_guard<std::mutex> lock(mutex_);
  stats_.bytes_per_second = 0;
  stats_.bytes_sent_current = 0;
  stats_.total_bytes_uploaded = 0;
  start_ticks_ = clock();
}

// Upload thread.  Wakes when user provides a buffer via call to
// |UploadBuffer|.
void HttpUploaderImpl::UploadThread() {
  while (!StopRequested() || upload_buffer_.GetNumBuffers() > 0) {
    BufferQueue::Buffer* buffer = upload_buffer_.DequeueBuffer();
    if (!buffer) {
      VLOG(1) << "upload thread waiting for buffer...";
      WaitForUserData();
      continue;
    }
    VLOG(1) << "uploading buffer...";
    if (!Upload(buffer)) {
      LOG(ERROR) << "buffer upload failed!";
      // TODO(tomfinegan): Report upload failure, and provide access to
      //                   response code and data.
    }
  }
  LOG(INFO) << "thread done";
}

void HttpUploaderImpl::FreeHeaders() {
  if (ptr_headers_) {
    curl_slist_free_all(ptr_headers_);
    ptr_headers_ = NULL;
  }
}

}  // namespace webmlive
