// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef HTTP_CLIENT_DEBUG_UTIL_H_
#define HTTP_CLIENT_DEBUG_UTIL_H_

#ifdef _WIN32
#include "webmdshow/common/odbgstream.hpp"

// Extracts error from the HRESULT, and output its hex and decimal values.
#define \
    HRLOG(X) L" {" << #X << L"=" << X << L"/" << std::hex << X << std::dec \
    << L" (" << hrtext(X) << L")}"
// Converts 100ns units to seconds
#define REFTIMETOSECONDS(X) (static_cast<double>(X) / 10000000.0f)
#else  // _WIN32
#include <sstream>
do { \
    std::wostringstream wos; \
    wos << __FILE__ << "(" << __LINE__ << ") : ["__FUNCTION__"] " << X \
        << std::endl; \
    fprintf(stderr, "%ls", wos.str().c_str()); \
} while (0)
#endif  // _WIN32

#ifdef _WIN32
// Keeps the compiler quiet about do/while(0)'s (constant conditionals) used in
// log macros.
#pragma warning(disable:4127)
#endif  // _WIN32

#endif  // HTTP_CLIENT_DEBUG_UTIL_H_
