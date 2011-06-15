// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

// At present this file exists solely for the purposes of specifying link
// dependencies.  The point of this is to easily allow switching between
// different libcurl releases.  As a side benefit (since we're here anyway)
// we specify all link dependencies here.  This way we don't need to muck
// around in vs prop pages when adding/removing deps.

// Note: since boost (at least appears to) link libs in this way, we still
// must specfify a path to boost libs in the vcproj file linker settings.

#pragma comment(lib, "../third_party/curl/win/x86/libcurl.lib")

// Windows API libraries
#pragma comment(lib, "shlwapi.lib")
