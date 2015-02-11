##  Copyright (c) 2015 The WebM project authors. All Rights Reserved.
##
##  Use of this source code is governed by a BSD-style license
##  that can be found in the LICENSE file in the root of the source
##  tree. An additional intellectual property rights grant can be found
##  in the file PATENTS.  All contributing project authors may
##  be found in the AUTHORS file in the root of the source tree.
cmake_minimum_required(VERSION 2.8)
project(GLOG)
include("${CMAKE_CURRENT_SOURCE_DIR}/../../build/msvc_runtime.cmake")

set(GLOG_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/src")
set(GLOG_WIN_INC_DIR "${GLOG_SRC_DIR}/windows/glog")

add_library(google-glog STATIC
            "${GLOG_SRC_DIR}/base/commandlineflags.h"
            "${GLOG_WIN_INC_DIR}/../config.h"
            "${GLOG_SRC_DIR}/base/googleinit.h"
            "${GLOG_WIN_INC_DIR}/log_severity.h"
            "${GLOG_WIN_INC_DIR}/logging.h"
            "${GLOG_SRC_DIR}/base/mutex.h"
            "${GLOG_WIN_INC_DIR}/../port.h"
            "${GLOG_WIN_INC_DIR}/raw_logging.h"
            "${GLOG_WIN_INC_DIR}/stl_logging.h"
            "${GLOG_WIN_INC_DIR}/vlog_is_on.h"
            "${GLOG_SRC_DIR}/logging.cc"
            "${GLOG_SRC_DIR}/windows/port.cc"
            "${GLOG_SRC_DIR}/raw_logging.cc"
            "${GLOG_SRC_DIR}/utilities.cc"
            "${GLOG_SRC_DIR}/vlog_is_on.cc")

include_directories("${GLOG_SRC_DIR}"
                    "${GLOG_SRC_DIR}/base"
                    "${GLOG_WIN_INC_DIR}/.."
                    "${GLOG_WIN_INC_DIR}")
