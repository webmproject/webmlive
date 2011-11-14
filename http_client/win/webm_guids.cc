// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "http_client/win/webm_guids.h"

// 30323449-0000-0010-8000-00AA00389B71 'I420'
const GUID webmlive::MEDIASUBTYPE_I420 = {
  0x30323449,
  0x0000,
  0x0010,
  { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 }
};

// {D0DBABEA-71A5-40fb-95F1-7E0E3C1407E6}
const CLSID webmlive::CLSID_VideoSinkFilter =  {
  0xd0dbabea,
  0x71a5,
  0x40fb,
  { 0x95, 0xf1, 0x7e, 0xe, 0x3c, 0x14, 0x7, 0xe6 }
};
