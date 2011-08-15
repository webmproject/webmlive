// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef HTTP_CLIENT_HTTP_CLIENT_BASE_H_
#define HTTP_CLIENT_HTTP_CLIENT_BASE_H_

#if _WIN32

#ifndef _WIN32_WINNT
#  define _WIN32_WINNT 0x0501  // WinXP
#endif

// Disable Microsoft's "safety" warnings.  "Fixing" them results non-portable
// code that is no safer.
#pragma warning(disable:4996)

#include "windows.h"

#endif  // _WIN32

#endif  // HTTP_CLIENT_HTTP_CLIENT_BASE_H_
