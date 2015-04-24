// Copyright (c) 2015 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "encoder/capture_source_list.h"

#include <sstream>

#include "glog/logging.h"

#include "encoder/win/media_source_dshow.h"
#include "encoder/win/string_util_win.h"

class AutoComInit {
 public:
  AutoComInit() { CoInitialize(NULL); }
  ~AutoComInit() { CoUninitialize(); }
};

namespace webmlive {

std::string GetAudioSourceList() {
  AutoComInit com_init;
  CaptureSourceLoader loader;
  int status = loader.Init(CLSID_AudioInputDeviceCategory);
  if (status) {
    LOG(ERROR) << "no video source!";
    return "";
  }

  std::ostringstream aud_list;
  for (int i = 0; i < loader.GetNumSources(); ++i) {
    const std::string dev_name =
        WStringToString(loader.GetSourceName(i).c_str());
    LOG(INFO) << "adev" << i << ": " << dev_name;
    aud_list << i << ": " << dev_name << "\n";
  }

  return aud_list.str();
}

std::string GetVideoSourceList() {
  AutoComInit com_init;
  CaptureSourceLoader loader;
  int status = loader.Init(CLSID_VideoInputDeviceCategory);
  if (status) {
    LOG(ERROR) << "no video source!";
    return "";
  }

  std::ostringstream vid_list;
  for (int i = 0; i < loader.GetNumSources(); ++i) {
    const std::string dev_name =
        WStringToString(loader.GetSourceName(i).c_str());
    LOG(INFO) << "vdev" << i << ": " << dev_name;
    vid_list << i << ": " << dev_name << "\n";
  }

  return vid_list.str();
}




}  // namespace webmlive
