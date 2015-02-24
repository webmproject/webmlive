##  Copyright (c) 2015 The WebM project authors. All Rights Reserved.
##
##  Use of this source code is governed by a BSD-style license
##  that can be found in the LICENSE file in the root of the source
##  tree. An additional intellectual property rights grant can be found
##  in the file PATENTS.  All contributing project authors may
##  be found in the AUTHORS file in the root of the source tree.
cmake_minimum_required(VERSION 2.8)
project(DIRECTSHOW_BASECLASSES)
include("${CMAKE_CURRENT_SOURCE_DIR}/../../build/msvc_runtime.cmake")

set(DSHOW_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/baseclasses")
add_library(dshow_baseclasses STATIC
            "${DSHOW_SRC_DIR}/amextra.cpp"
            "${DSHOW_SRC_DIR}/amextra.h"
            "${DSHOW_SRC_DIR}/amfilter.cpp"
            "${DSHOW_SRC_DIR}/amfilter.h"
            "${DSHOW_SRC_DIR}/amvideo.cpp"
            "${DSHOW_SRC_DIR}/arithutil.cpp"
            "${DSHOW_SRC_DIR}/cache.h"
            "${DSHOW_SRC_DIR}/checkbmi.h"
            "${DSHOW_SRC_DIR}/combase.cpp"
            "${DSHOW_SRC_DIR}/combase.h"
            "${DSHOW_SRC_DIR}/cprop.cpp"
            "${DSHOW_SRC_DIR}/cprop.h"
            "${DSHOW_SRC_DIR}/ctlutil.cpp"
            "${DSHOW_SRC_DIR}/ctlutil.h"
            "${DSHOW_SRC_DIR}/ddmm.cpp"
            "${DSHOW_SRC_DIR}/ddmm.h"
            "${DSHOW_SRC_DIR}/dllentry.cpp"
            "${DSHOW_SRC_DIR}/dllsetup.cpp"
            "${DSHOW_SRC_DIR}/dllsetup.h"
            "${DSHOW_SRC_DIR}/dxmperf.h"
            "${DSHOW_SRC_DIR}/fourcc.h"
            "${DSHOW_SRC_DIR}/measure.h"
            "${DSHOW_SRC_DIR}/msgthrd.h"
            "${DSHOW_SRC_DIR}/mtype.cpp"
            "${DSHOW_SRC_DIR}/mtype.h"
            "${DSHOW_SRC_DIR}/outputq.cpp"
            "${DSHOW_SRC_DIR}/outputq.h"
            "${DSHOW_SRC_DIR}/perflog.cpp"
            "${DSHOW_SRC_DIR}/perflog.h"
            "${DSHOW_SRC_DIR}/perfstruct.h"
            "${DSHOW_SRC_DIR}/pstream.cpp"
            "${DSHOW_SRC_DIR}/pstream.h"
            "${DSHOW_SRC_DIR}/pullpin.cpp"
            "${DSHOW_SRC_DIR}/pullpin.h"
            "${DSHOW_SRC_DIR}/refclock.cpp"
            "${DSHOW_SRC_DIR}/refclock.h"
            "${DSHOW_SRC_DIR}/reftime.h"
            "${DSHOW_SRC_DIR}/renbase.cpp"
            "${DSHOW_SRC_DIR}/renbase.h"
            "${DSHOW_SRC_DIR}/schedule.cpp"
            "${DSHOW_SRC_DIR}/schedule.h"
            "${DSHOW_SRC_DIR}/seekpt.cpp"
            "${DSHOW_SRC_DIR}/seekpt.h"
            "${DSHOW_SRC_DIR}/source.cpp"
            "${DSHOW_SRC_DIR}/source.h"
            "${DSHOW_SRC_DIR}/streams.h"
            "${DSHOW_SRC_DIR}/strmctl.cpp"
            "${DSHOW_SRC_DIR}/strmctl.h"
            "${DSHOW_SRC_DIR}/sysclock.cpp"
            "${DSHOW_SRC_DIR}/sysclock.h"
            "${DSHOW_SRC_DIR}/transfrm.cpp"
            "${DSHOW_SRC_DIR}/transfrm.h"
            "${DSHOW_SRC_DIR}/transip.cpp"
            "${DSHOW_SRC_DIR}/transip.h"
            "${DSHOW_SRC_DIR}/videoctl.cpp"
            "${DSHOW_SRC_DIR}/videoctl.h"
            "${DSHOW_SRC_DIR}/vtrans.cpp"
            "${DSHOW_SRC_DIR}/vtrans.h"
            "${DSHOW_SRC_DIR}/winctrl.cpp"
            "${DSHOW_SRC_DIR}/winctrl.h"
            "${DSHOW_SRC_DIR}/winutil.cpp"
            "${DSHOW_SRC_DIR}/winutil.h"
            "${DSHOW_SRC_DIR}/wxdebug.cpp"
            "${DSHOW_SRC_DIR}/wxdebug.h"
            "${DSHOW_SRC_DIR}/wxlist.cpp"
            "${DSHOW_SRC_DIR}/wxlist.h"
            "${DSHOW_SRC_DIR}/wxutil.cpp"
            "${DSHOW_SRC_DIR}/wxutil.h")

include_directories("${DSHOW_SRC_DIR}")
