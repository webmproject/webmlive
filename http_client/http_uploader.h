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
  // Target for HTTP POST.
  std::string target_url;
  typedef std::map<std::string, std::string> StringMap;
  // User form variables.
  StringMap form_variables;
  // User HTTP headers.
  StringMap headers;
};

struct HttpUploaderStats {
  // Upload average bytes per second.
  double bytes_per_second;
  // Total bytes sent.
  int64 bytes_sent;
};

class HttpUploaderImpl;

// Pimpl idiom based HTTP uploader. The reason the implementation is hidden is
// mainly to avoid shoving libcurl in the face of all code using the uploader.
class HttpUploader {
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
  ~HttpUploader();
  // Test for upload completion. Returns true when the uploader is ready to
  // start an upload. Always true when no uploads have been attempted.
  bool UploadComplete();
  // Initialize the uploader.
  int Init(HttpUploaderSettings* ptr_settings);
  // Return the current upload stats. Note, obtains lock before copying stats to
  // |ptr_stats|.
  int GetStats(HttpUploaderStats* ptr_stats);
  // Run the uploader thread.
  int Run();
  // Stop the uploader thread.
  int Stop();
  // Send a buffer to the uploader thread.
  int UploadBuffer(const uint8* const ptr_buffer, int32 length);
  // TODO(tomfinegan): Add UploadFile for upload of existing files. This will
  //                   complicate the upload thread, but will be worth it when
  //                   upload of existing files is implemented.
 private:
  // Pointer to uploader implementation.
  boost::scoped_ptr<HttpUploaderImpl> ptr_uploader_;
  DISALLOW_COPY_AND_ASSIGN(HttpUploader);
};

} // WebmLive

#endif // WEBMLIVE_HTTP_UPLOADER_H
