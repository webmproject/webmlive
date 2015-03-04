// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "encoder/win/webm_guids.h"

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

// {D0DBABEA-71A5-40fb-95F1-7E0E3C1407E6}
const CLSID webmlive::CLSID_KsDataTypeHandlerVideo = {
  0x05589f80,
  0xc356,
  0x11ce,
  { 0xbf, 0x01, 0x00, 0xaa, 0x00, 0x55, 0x59, 0x5a }
};

// 30385056-0000-0010-8000-00AA00389B71 'VP80'
const GUID webmlive::MEDIASUBTYPE_VP80 = {
  0x30385056,
  0x0000,
  0x0010,
  { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 }
};

// {3B36472A-9643-40a5-BA82-A4023B63807B}
const GUID webmlive::CLSID_AudioSinkFilter = {
  0x3b36472a,
  0x9643,
  0x40a5,
  { 0xba, 0x82, 0xa4, 0x2, 0x3b, 0x63, 0x80, 0x7b }
};
