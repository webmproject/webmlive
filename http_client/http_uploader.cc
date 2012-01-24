// Copyright (c) 2012 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "http_client/http_uploader.h"

#include <ctime>
#include <queue>
#include <string>
#include <vector>

#include "boost/shared_ptr.hpp"
#include "boost/thread/condition.hpp"
#include "boost/thread/thread.hpp"
#include "curl/curl.h"
#include "curl/types.h"
#include "curl/easy.h"
#include "glog/logging.h"
#include "http_client/buffer_util.h"
#include "libwebm/mkvparser.hpp"

#define LOG_CURL_ERR(CURL_ERR, MSG_STR) \
  LOG(ERROR) << MSG_STR << " err=" << CURL_ERR << ":" \
             << curl_easy_strerror(CURL_ERR)
#define LOG_CURLFORM_ERR(CURL_ERR, MSG_STR) \
  LOG(ERROR) << MSG_STR << " err=" << CURL_ERR

namespace webmlive {

static const char* kExpectHeader = "Expect:";
static const char* kContentTypeHeader = "Content-Type: video/webm";
static const char* kFormName = "webm_file";
static const char* kWebmMimeType = "video/webm";
static const int kUnknownFileSize = -1;
static const int kBytesRequiredForResume = 32*1024;

class HttpUploaderImpl {
 public:
  typedef std::queue<std::string> UrlQueue;
  enum {
    // Libcurl reported an unexpected error.
    kLibCurlError = -401,
    kSuccess = 0,

    // Constant value used to stop libcurl when |StopRequested| returns true
    // in |WriteCallback|.
    kWriteCallbackStopRequest = 0,

    // Constant value used to stop libcurl when |StopRequested| returns true
    // in |ProgressCallback|.
    kProgressCallbackStopRequest = 1,

    // Returned by |Upload| when |WaitForUserData| was notified with an
    // unlocked |upload_buffer_|, which means |Stop| is waiting for
    // |UploadThread| to exit.
    kStopping = 2,
  };

  HttpUploaderImpl();
  ~HttpUploaderImpl();

  // Returns true when the uploader is ready to start an upload. Always returns
  // true when no uploads have been attempted.
  bool UploadComplete() const;

  // Copies user settings and configures libcurl.
  int Init(const HttpUploaderSettings& settings);

  // Locks |mutex_| and copies current stats to |ptr_stats|.
  int GetStats(HttpUploaderStats* ptr_stats);

  // Runs |UploadThread|, and starts waiting for user data.
  int Run();

  // Uploads user data.
  int UploadBuffer(const uint8* ptr_buffer, int32 length);

  // Stops the uploader.
  int Stop();

  // Adds |target_url| to |url_queue_|. Each time |UploadBuffer| is called, an
  // URL is popped off the queue and assigned to |target_url_|
  void EnqueueTargetUrl(const std::string& target_url);

 private:
  // Used by |UploadThread|. Returns true if user has called |Stop|.
  bool StopRequested();

  // Pass our callbacks, |ProgressCallback| and |WriteCallback|, to libcurl.
  CURLcode SetCurlCallbacks();

  // Pass user HTTP headers to libcurl, and disable HTTP 100 responses.
  CURLcode SetHeaders();

  // Configures libcurl to POST data buffers as file data in a form/multipart
  // HTTP POST.
  int SetupFormPost(const uint8* const ptr_buffer, int32 length);

  // Configures libcurl to POST data buffers as HTTP POST content-data.
  int SetupPost(const uint8* const ptr_buffer, int32 length);

  // Upload user data with libcurl.
  int Upload();

  // Wakes up |UploadThread| when users pass data through |UploadBuffer|.
  int WaitForUserData();

  // Libcurl progress callback function.  Acquires |mutex_| and updates
  // |stats_|.
  static int ProgressCallback(void* ptr_this,
                              double, double,  // we ignore download progress
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

  // Stop flag. Internal callers use |StopRequested| to allow for
  // synchronization via |mutex_|.  Set by |Stop|, and responded to in
  // |UploadThread|.
  bool stop_;

  // Upload complete/ready to upload flag.  Initializes to true to allow
  // users of the uploader to base all Upload calls on |UploadComplete|.
  bool upload_complete_;

  // Condition variable used to wake |UploadThread| when a user code passes a
  // buffer to |UploadBuffer|.
  boost::condition_variable buffer_ready_;

  // Mutex for synchronization of public method calls with |UploadThread|
  // activity. Mutable so |UploadComplete()| can be a const method.
  mutable boost::mutex mutex_;

  // Thread object.
  boost::shared_ptr<boost::thread> upload_thread_;

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
  LockableBuffer upload_buffer_;

  // The name of the file on the local system.  Note that it is not being read,
  // it's information included within the form data contained within the HTTP
  // post.
  std::string local_file_name_;

  // Last URL read from |url_queue_|. Used repeatedly once |url_queue_| is
  // empty.
  std::string target_url_;

  // Queue of target URLs.
  UrlQueue url_queue_;

  WEBMLIVE_DISALLOW_COPY_AND_ASSIGN(HttpUploaderImpl);
};

///////////////////////////////////////////////////////////////////////////////
// HttpUploader
//

HttpUploader::HttpUploader() {
}

HttpUploader::~HttpUploader() {
}

// Return result of |UploadeComplete| on |ptr_uploader_|.
bool HttpUploader::UploadComplete() const {
  return ptr_uploader_->UploadComplete();
}

// Copy user settings, and setup the internal uploader object.
int HttpUploader::Init(const HttpUploaderSettings& settings) {
  ptr_uploader_.reset(new (std::nothrow) HttpUploaderImpl());  // NOLINT
  if (!ptr_uploader_) {
    LOG(ERROR) << "can't construct HttpUploaderImpl.";
    return kInitFailed;
  }
  int status = ptr_uploader_->Init(settings);
  if (status) {
    LOG(ERROR) << "uploader init failed. " << status;
    return kInitFailed;
  }
  return kSuccess;
}

// Return result of |GetStats| on |ptr_uploader_|.
int HttpUploader::GetStats(webmlive::HttpUploaderStats* ptr_stats) {
  return ptr_uploader_->GetStats(ptr_stats);
}

// Return result of |Run| on |ptr_uploader_|.
int HttpUploader::Run() {
  return ptr_uploader_->Run();
}

// Return result of |Stop| on |ptr_uploader_|.
int HttpUploader::Stop() {
  return ptr_uploader_->Stop();
}

// Return result of |UploadBuffer| on |ptr_uploader_|.
int HttpUploader::UploadBuffer(const uint8* ptr_buffer, int32 length) {
  return ptr_uploader_->UploadBuffer(ptr_buffer, length);
}

void HttpUploader::EnqueueTargetUrl(const std::string& target_url) {
  ptr_uploader_->EnqueueTargetUrl(target_url);
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
  if (ptr_headers_) {
    curl_slist_free_all(ptr_headers_);
    ptr_headers_ = NULL;
  }
}

// Obtain lock on |mutex_| and return value of |upload_complete_|.
bool HttpUploaderImpl::UploadComplete() const {
  bool complete = false;
  boost::mutex::scoped_try_lock lock(mutex_);
  if (lock.owns_lock()) {
    complete = upload_complete_;
  }
  return complete;
}

// Initializes the upload:
// - copies user settings
// - sets basic libcurl settings (progress and write callbacks)
// - calls SetHeaders to pass user headers to libcurl
int HttpUploaderImpl::Init(const HttpUploaderSettings& settings) {
  // copy user settings
  settings_ = settings;

  // Init libcurl.
  ptr_curl_ = curl_easy_init();
  if (!ptr_curl_) {
    LOG(ERROR) << "curl_easy_init failed!";
    return kLibCurlError;
  }

  // Enable progress reports from libcurl.
  CURLcode curl_ret = curl_easy_setopt(ptr_curl_, CURLOPT_NOPROGRESS, FALSE);
  if (curl_ret != CURLE_OK) {
    LOG_CURL_ERR(curl_ret, "curl progress enable failed.");
    return kLibCurlError;
  }

  // Set callbacks.
  curl_ret = SetCurlCallbacks();
  if (curl_ret != CURLE_OK) {
    LOG_CURL_ERR(curl_ret, "curl callback setup failed.");
    return kLibCurlError;
  }

  // Disable HTTP 100 responses, and set user HTTP headers.
  curl_ret = SetHeaders();
  if (curl_ret) {
    LOG_CURL_ERR(curl_ret, "unable to set headers.");
    return HttpUploader::kHeaderError;
  }

  local_file_name_ = settings_.local_file;
  ResetStats();
  return kSuccess;
}

// Obtain lock on |mutex_| and copy current stats values from |stats_| to
// |ptr_stats|.
int HttpUploaderImpl::GetStats(HttpUploaderStats* ptr_stats) {
  if (!ptr_stats) {
    LOG(ERROR) << "NULL ptr_stats";
    return HttpUploader::kInvalidArg;
  }
  boost::mutex::scoped_lock lock(mutex_);
  ptr_stats->bytes_per_second = stats_.bytes_per_second;
  ptr_stats->bytes_sent_current = stats_.bytes_sent_current;
  ptr_stats->total_bytes_uploaded = stats_.total_bytes_uploaded;
  return kSuccess;
}

// Run |UploadThread| using |boost::thread|.
int HttpUploaderImpl::Run() {
  assert(!upload_thread_);
  using boost::bind;
  using boost::shared_ptr;
  using boost::thread;
  using std::nothrow;
  upload_thread_ = shared_ptr<thread>(
      new (nothrow) thread(bind(&HttpUploaderImpl::UploadThread,  // NOLINT
                                this)));
  return kSuccess;
}

// Try to obtain lock on |mutex_|, and upload the user buffer stored in
// |upload_buffer_| if the buffer is unlocked.  If the lock is obtained and the
// buffer is unlocked, |UploadBuffer| locks the buffer and notifies the upload
// thread through call to |notify_one| on the |buffer_ready_| condition
// variable.
int HttpUploaderImpl::UploadBuffer(const uint8* ptr_buf, int32 length) {
  int status = HttpUploader::kUploadInProgress;
  boost::mutex::scoped_try_lock lock(mutex_);
  if (lock.owns_lock() && !upload_buffer_.IsLocked()) {
    if (!url_queue_.empty()) {
      target_url_ = url_queue_.front();
    }
    if (target_url_.empty()) {
      LOG(ERROR) << "No target URL!";
      return HttpUploader::kUrlConfigError;
    }

    // Lock obtained; (re)initialize |upload_buffer_| with the user data...
    status = upload_buffer_.Init(ptr_buf, length);
    if (status) {
      LOG(ERROR) << "upload_buffer_ Init failed, status=" << status;
      return status;
    }

    // Lock |upload_buffer_|; it's unlocked by |UploadThread| once libcurl
    // finishes its run.
    status = upload_buffer_.Lock();
    if (status) {
      LOG(ERROR) << "upload_buffer_ Lock failed, status=" << status;
      return status;
    }
    upload_complete_ = false;

    // Wake |UploadThread|.
    LOG(INFO) << "waking uploader with " << length << " bytes";
    buffer_ready_.notify_one();
  }
  return status;
}

// Stops |UploadThread|. First it wakes the thread by calling |notify_one| on
// the |buffer_ready_| condition variable without locking |upload_buffer_|,
// which causes |Upload| to return |kStopping| to |UploadThread|, breaking the
// loop. This takes care of stopping if the uploader was waiting for user data
// in |WaitForUserData|.
// It then obtains lock on |mutex_|, sets |stop_| to true, and releases lock to
// ensure a running upload stops when |StopRequested| is called within the
// libcurl callbacks.
int HttpUploaderImpl::Stop() {
  assert(upload_thread_);
  if (UploadComplete()) {
    // Wake up the upload thread
    buffer_ready_.notify_one();
  }
  boost::mutex::scoped_lock lock(mutex_);
  stop_ = true;
  lock.unlock();
  upload_thread_->join();
  return kSuccess;
}

void HttpUploaderImpl::EnqueueTargetUrl(const std::string& target_url) {
  boost::mutex::scoped_lock(mutex_);
  url_queue_.push(target_url);
}

// Try to obtain lock on |mutex_|, and return the value of |stop_| if lock is
// obtained.  Returns false if unable to obtain the lock.
bool HttpUploaderImpl::StopRequested() {
  bool stop_requested = false;
  boost::mutex::scoped_try_lock lock(mutex_);
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
CURLcode HttpUploaderImpl::SetHeaders() {
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
  CURLcode err = curl_easy_setopt(ptr_curl_, CURLOPT_HTTPHEADER, ptr_headers_);
  if (err != CURLE_OK) {
    LOG_CURL_ERR(err, "setopt CURLOPT_HTTPHEADER failed err=");
  }
  return err;
}

// Sets necessary curl options for form based file upload, and adds the user
// form variables.
int HttpUploaderImpl::SetupFormPost(const uint8* const ptr_buffer,
                                    int32 length) {
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
      return HttpUploader::kFormError;
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
    return err;
  }
  // pass the form to libcurl
  CURLcode err_setopt = curl_easy_setopt(ptr_curl_, CURLOPT_HTTPPOST,
                                         ptr_form_);
  if (err_setopt != CURLE_OK) {
    LOG_CURL_ERR(err_setopt, "setopt CURLOPT_HTTPPOST failed.");
    return err_setopt;
  }
  return kSuccess;
}

// Configures libcurl to POST data buffers as HTTP POST content-data.
int HttpUploaderImpl::SetupPost(const uint8* const ptr_buffer, int32 length) {
  CURLcode err_setopt = curl_easy_setopt(ptr_curl_, CURLOPT_POST, ptr_form_);
  if (err_setopt != CURLE_OK) {
    LOG_CURL_ERR(err_setopt, "setopt CURLOPT_HTTPPOST failed.");
    return err_setopt;
  }
  // Pass |ptr_buffer| to libcurl; it's used in the call to |curl_easy_perform|
  err_setopt = curl_easy_setopt(ptr_curl_, CURLOPT_POSTFIELDS, ptr_buffer);
  if (err_setopt != CURLE_OK) {
    LOG_CURL_ERR(err_setopt, "setopt CURLOPT_POSTFIELDS failed.");
    return err_setopt;
  }
  // Tell libcurl the size of |ptr_buffer|.  If libcurl is not informed of the
  // size before the call to |curl_easy_perform|, it will use strlen to
  // determine the length of the data.
  err_setopt = curl_easy_setopt(ptr_curl_, CURLOPT_POSTFIELDSIZE, length);
  if (err_setopt != CURLE_OK) {
    LOG_CURL_ERR(err_setopt, "setopt CURLOPT_POSTFIELDSIZE failed.");
    return err_setopt;
  }
  return kSuccess;
}

// Upload data using libcurl.
int HttpUploaderImpl::Upload() {
  if (!upload_buffer_.IsLocked()) {
    LOG(INFO) << "woke with unlocked buffer, stopping.";
    return kStopping;
  }

  uint8* ptr_data = NULL;
  int32 length = 0;
  int status = upload_buffer_.GetBuffer(&ptr_data, &length);
  if (status) {
    LOG(ERROR) << "error, could not get buffer pointer, status=" << status;
    return HttpUploader::kRunFailed;
  }

  LOG(INFO) << "upload buffer size=" << length;
  CURLcode err = curl_easy_setopt(ptr_curl_, CURLOPT_URL,
                                  target_url_.c_str());
  if (err != CURLE_OK) {
    LOG_CURL_ERR(err, "could not pass URL to curl.");
    return HttpUploader::kUrlConfigError;
  }

  if (settings_.post_mode == webmlive::HTTP_FORM_POST) {
    if (SetupFormPost(ptr_data, length)) {
      LOG(ERROR) << "SetupFormPost failed!";
      return HttpUploader::kRunFailed;
    }
  } else {
    if (SetupPost(ptr_data, length)) {
      LOG(ERROR) << "SetupPost failed!";
      return HttpUploader::kRunFailed;
    }
  }

  err = curl_easy_perform(ptr_curl_);
  if (err != CURLE_OK) {
    LOG_CURL_ERR(err, "curl_easy_perform failed.");
  } else {
    int resp_code = 0;
    curl_easy_getinfo(ptr_curl_, CURLINFO_RESPONSE_CODE, &resp_code);
    LOG(INFO) << "server response code: " << resp_code;

    // Upload was successful, pop the target url off of the queue.
    if (!url_queue_.empty()) {
      url_queue_.pop();
    }
  }

  // Update total bytes uploaded.
  double bytes_uploaded = 0;
  err = curl_easy_getinfo(ptr_curl_, CURLINFO_SIZE_UPLOAD, &bytes_uploaded);
  if (err != CURLE_OK) {
    LOG_CURL_ERR(err, "curl_easy_getinfo CURLINFO_SIZE_UPLOAD failed.");
  } else {
    boost::mutex::scoped_lock lock(mutex_);
    stats_.bytes_sent_current = 0;
    stats_.total_bytes_uploaded += static_cast<int64>(bytes_uploaded);
  }

  return kSuccess;
}

// Idle the upload thread while awaiting user data.
int HttpUploaderImpl::WaitForUserData() {
  boost::mutex::scoped_lock lock(mutex_);
  buffer_ready_.wait(lock);  // Unlock |mutex_| and idle the thread while we
                             // wait for the next chunk of user data.
  return kSuccess;
}

// Handle libcurl progress updates.
int HttpUploaderImpl::ProgressCallback(void* ptr_this,
                                       double download_total,
                                       double download_current,
                                       double upload_total,
                                       double upload_current) {
  // Ignore the download progress variables.
  download_total;
  download_current;
  HttpUploaderImpl* ptr_uploader_ =
    reinterpret_cast<HttpUploaderImpl*>(ptr_this);
  if (ptr_uploader_->StopRequested()) {
    LOG(ERROR) << "stop requested.";
    return kProgressCallbackStopRequest;
  }
  boost::mutex::scoped_lock lock(ptr_uploader_->mutex_);
  HttpUploaderStats& stats = ptr_uploader_->stats_;
  stats.bytes_sent_current = static_cast<int64>(upload_current);
  double ticks_elapsed = clock() - ptr_uploader_->start_ticks_;
  double ticks_per_sec = CLOCKS_PER_SEC;
  stats.bytes_per_second =
      (upload_current + stats.total_bytes_uploaded) /
      (ticks_elapsed / ticks_per_sec);
  VLOG(4) << "total=" << static_cast<int>(upload_total) << " bytes_per_sec="
          << static_cast<int>(stats.bytes_per_second);
  return 0;
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
    return kWriteCallbackStopRequest;
  }
  return size*nitems;
}

// Reset uploaded byte count, and store upload start time.
void HttpUploaderImpl::ResetStats() {
  boost::mutex::scoped_lock lock(mutex_);
  stats_.bytes_per_second = 0;
  stats_.bytes_sent_current = 0;
  stats_.total_bytes_uploaded = 0;
  start_ticks_ = clock();
}

// Upload thread.  Wakes when user provides a buffer via call to
// |UploadBuffer|.
void HttpUploaderImpl::UploadThread() {
  LOG(INFO) << "upload thread running...";
  while (!StopRequested()) {
    LOG(INFO) << "upload thread waiting for buffer...";
    WaitForUserData();
    LOG(INFO) << "uploading buffer...";
    int status = Upload();
    if (status == kStopping) {
      break;
    }
    if (status) {
      LOG(ERROR) << "buffer upload failed, status=" << status;
      // TODO(tomfinegan): Report upload failure, and provide access to
      //                   response code and data.
    } else {
      boost::mutex::scoped_lock lock(mutex_);
      LOG(INFO) << "unlocking upload buffer...";
      status = upload_buffer_.Unlock();
      if (status) {
        LOG(ERROR) << "unable to unlock buffer, status=" << status;
        // keep spinning, for now...
      }
      upload_complete_ = true;
    }
  }
  LOG(INFO) << "thread done";
}

}  // namespace webmlive
