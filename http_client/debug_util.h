// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef WEBMLIVE_DEBUGUTIL_H
#define WEBMLIVE_DEBUGUTIL_H

#pragma once

#ifdef _DEBUG
#include "webmdshow/common/odbgstream.hpp"

// Simple trace logging macro that expands to nothing in release mode builds.
// Output is sent to the vs console.
#define DBGLOG(X) \
do { \
    wodbgstream wos; \
    wos << "["__FUNCTION__"] " << X << std::endl; \
} while(0)

// Extract error from the HRESULT, and output its hex and decimal values.
#define \
    HRLOG(X) L" {" << #X << L"=" << X << L"/" << std::hex << X << std::dec \
    << L" (" << hrtext(X) << L")}"

// Convert 100ns units to seconds
#define REFTIMETOSECONDS(X) (double(X) / 10000000.0f)

#else
#define DBGLOG(X)
#define REFTIMETOSECONDS(X)
#endif

// Keep the compiler quiet about do/while(0)'s (constant conditional) used in
// log macros.
#pragma warning(disable:4127)

// Check the HRESULT for failure (<0), and log it if we're in debug mode, and
// format the failure text so that it is clickable in vs output window.
#define CHK(X, Y) \
do { \
    if (FAILED(X=(Y))) \
    { \
        DBGLOG("\n" << __FILE__ << "(" << __LINE__ << ") : " << #Y << HRLOG(X)); \
    } \
} while (0)

#endif // WEBMLIVE_DEBUGUTIL_H
