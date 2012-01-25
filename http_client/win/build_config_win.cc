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

#pragma comment(lib, "../third_party/curl/win/x86/libcurldll.a")

#ifdef _DEBUG
#pragma comment(lib, "../third_party/glog/win/x86/debug/libglog_static.lib")
#pragma comment(lib, "../third_party/libwebm/win/x86/debug/libwebm.lib")
#pragma comment(lib, "../third_party/libogg/win/x86/debug/libogg_static.lib")
#pragma comment(lib, "../third_party/libvorbis/win/x86/debug/libvorbis_static.lib")
#pragma comment(lib, "../third_party/libvpx/win/x86/debug/vpxmtd.lib")
#pragma comment(lib, "../third_party/libyuv/win/x86/debug/libyuv.lib")
#else
#pragma comment(lib, "../third_party/glog/win/x86/release/libglog_static.lib")
#pragma comment(lib, "../third_party/libwebm/win/x86/release/libwebm.lib")
#pragma comment(lib, "../third_party/libogg/win/x86/release/libogg_static.lib")
#pragma comment(lib, "../third_party/libvorbis/win/x86/release/libvorbis_static.lib")
#pragma comment(lib, "../third_party/libvpx/win/x86/release/vpxmt.lib")
#pragma comment(lib, "../third_party/libyuv/win/x86/release/libyuv.lib")
#endif

// Windows API libraries
#pragma comment(lib, "quartz.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "strmiids.lib")
#pragma comment(lib, "winmm.lib")
