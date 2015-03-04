// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef WEBMLIVE_ENCODER_WIN_WEBM_GUIDS_H_
#define WEBMLIVE_ENCODER_WIN_WEBM_GUIDS_H_

#include <objbase.h>

namespace webmlive {

extern const CLSID CLSID_AudioSinkFilter;
extern const CLSID CLSID_VideoSinkFilter;
extern const CLSID CLSID_KsDataTypeHandlerVideo;
extern const GUID MEDIASUBTYPE_I420;
extern const GUID MEDIASUBTYPE_VP80;

}  // namespace webmlive

#endif  // WEBMLIVE_ENCODER_WIN_WEBM_GUIDS_H_
