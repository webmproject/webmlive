// Copyright (c) 2012 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef WEBMLIVE_ENCODER_HTTP_UPLOADER_H_
#define WEBMLIVE_ENCODER_HTTP_UPLOADER_H_

#include <map>
#include <memory>
#include <queue>
#include <string>

#include "encoder/basictypes.h"
#include "encoder/data_sink.h"
#include "encoder/encoder_base.h"

namespace webmlive {

enum UploadMode {
  HTTP_POST = 0,
  HTTP_FORM_POST = 1,
};

struct HttpUploaderSettings {
  // Form variables and HTTP headers are stored within
  // map<std::string,std::string>.
  typedef std::map<std::string, std::string> StringMap;

  // |local_file| is what the HTTP server sees as the local file name.
  // Assigning a path to a local file and passing the settings struct to
  // |HttpUploader::Init| will not upload an existing file.
  std::string local_file;

  // User form variables.
  StringMap form_variables;

  // User HTTP headers.
  StringMap headers;

  // HTTP post data stream name.
  std::string stream_name;

  // Data stream ID.
  std::string stream_id;

  // Post mode
  UploadMode post_mode;
};

struct HttpUploaderStats {
  // Upload average bytes per second.
  double bytes_per_second;

  // Bytes sent for current upload.
  int64 bytes_sent_current;

  // Total number of bytes uploaded.
  int64 total_bytes_uploaded;
};

class HttpUploaderImpl;

// Pimpl idiom based HTTP uploader that hides the gory details of libcurl from
// users of the uploader.
//
// Notes:
// - |Init| must be called before any other method.
// - |EnqueueTargetUrl| must be used to control target for HTTP requests. URLs
//   enqueued are used in sequence, and only removed from the queue after
//   successful uploads.
class HttpUploader : public DataSinkInterface {
 public:
  enum {
    // Bad URL.
    kUrlConfigError = -307,

    // Bad user HTTP header, or error passing header to libcurl.
    kHeaderError = -305,

    // Bad user Form variable, or error passsing it to libcurl.
    kFormError = -304,

    // Invalid argument supplied to method call.
    kInvalidArg = -303,

    // Uploader |Init| failed.
    kInitFailed = -302,

    // Uploader |Run| failed.
    kRunFailed = -301,

    // Success.
    kSuccess = 0,

    // Upload already running.
    kUploadInProgress = 1,
  };
  HttpUploader();
  virtual ~HttpUploader();

  // Tests for upload completion. Returns true when the uploader is ready to
  // start an upload. Always returns true when no uploads have been attempted.
  bool UploadComplete() const;

  // Constructs |HttpUploaderImpl|, which copies |settings|. Returns |kSuccess|
  // upon success.
  int Init(const HttpUploaderSettings& settings);

  // Returns the current upload stats. Note, obtains lock before copying stats
  // to |ptr_stats|.
  int GetStats(HttpUploaderStats* ptr_stats);

  // Runs the uploader thread.
  int Run();

  // Stops the uploader thread.
  int Stop();

  // Sends a buffer to the uploader thread using an URL from |url_queue_|. Use
  // |EnqueueTargetUrl| to set target URLs.
  int UploadBuffer(const uint8* ptr_buffer, int32 length);

  // Calls |HttpUploaderImpl::EnqueueTargetUrl| to enqueue |target_url|.
  void EnqueueTargetUrl(const std::string& target_url);

  // DataSinkInterface methods.
  virtual bool Ready() const { return UploadComplete(); }
  virtual bool WriteData(const uint8* ptr_buffer, int32 length,
                         const std::string& id) {
    return (UploadBuffer(ptr_buffer, length) == kSuccess);
  }

 private:
  // Pointer to uploader implementation.
  std::unique_ptr<HttpUploaderImpl> ptr_uploader_;
  WEBMLIVE_DISALLOW_COPY_AND_ASSIGN(HttpUploader);
};

}  // namespace webmlive

#endif  // WEBMLIVE_ENCODER_HTTP_UPLOADER_H_
