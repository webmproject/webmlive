// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef HTTP_CLIENT_BASICTYPES_H_
#define HTTP_CLIENT_BASICTYPES_H_

typedef signed char         schar;
typedef signed char         int8;
typedef short               int16;   // NOLINT
typedef int                 int32;
typedef unsigned char       uint8;
typedef unsigned short      uint16;  // NOLINT
typedef unsigned int        uint32;

#if defined _MSC_VER || defined _WIN32
typedef __int64             int64;
typedef unsigned __int64    uint64;
#else
typedef long long           int64;   // NOLINT
typedef unsigned long long  uint64;  // NOLINT
#endif

#define WEBMLIVE_DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&);                        \
  void operator=(const TypeName&)

#endif  // HTTP_CLIENT_BASICTYPES_H_
