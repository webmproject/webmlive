##  Copyright (c) 2015 The WebM project authors. All Rights Reserved.
##
##  Use of this source code is governed by a BSD-style license
##  that can be found in the LICENSE file in the root of the source
##  tree. An additional intellectual property rights grant can be found
##  in the file PATENTS.  All contributing project authors may
##  be found in the AUTHORS file in the root of the source tree.
cmake_minimum_required(VERSION 2.8)
project(ENCODER)
include("${CMAKE_CURRENT_SOURCE_DIR}/../build/msvc_runtime.cmake")

#
# Build the target and config based portions of third party library paths.
#
# Detect Windows (and throw an error everywhere else).
if(WIN32)
  set(LIB_OS_NAME "win")
  # Disable inane MSVC warnings advising platform specific code changes.
  add_definitions("/wd4996")
  set(CMAKE_EXE_LINKER_FLAGS_RELEASE
      "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG /INCREMENTAL:NO /OPT:REF")
  set(CMAKE_MODULE_LINKER_FLAGS_RELEASE
      "${CMAKE_MODULE_LINKER_FLAGS_RELEASE} /LTCG /INCREMENTAL:NO /OPT:REF")
  set(CMAKE_SHARED_LINKER_FLAGS_RELEASE
      "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /LTCG /INCREMENTAL:NO /OPT:REF")
  set(STATIC_LIBRARY_FLAGS_RELEASE
      "${STATIC_LIBRARY_FLAGS_RELEASE} /LTCG /INCREMENTAL:NO /OPT:REF")
else(WIN32)
  error("The webmlive encoder supports only Windows.")
endif(WIN32)

# Use void pointer size to determine lib target name.
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(LIB_TARGET_NAME "x64")
else(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(LIB_TARGET_NAME "x86")
endif(CMAKE_SIZEOF_VOID_P EQUAL 8)

# Use build type to determine lib type.
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(LIB_TARGET_TYPE "debug")
else(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(LIB_TARGET_TYPE "release")
endif(CMAKE_BUILD_TYPE STREQUAL "Debug")

set(LIB_SUB_DIR "${LIB_OS_NAME}/${LIB_TARGET_NAME}/${LIB_TARGET_TYPE}")

#
# Build include and library paths.
#
set(THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third_party")

set(CURL_INCLUDE_DIR "${THIRD_PARTY_DIR}")
set(CURL_LIB_DIR "${CURL_INCLUDE_DIR}/curl/${LIB_SUB_DIR}")
# TODO(tomfinegan): Windows only, correct for other platforms.
set(CURL_LIB_NAME "libcurl.lib")

set(DSHOW_INCLUDE_DIR "${THIRD_PARTY_DIR}/directshow")

# TODO(tomfinegan): Windows only, correct for other platforms.
set(GLOG_WINDOWS_INCLUDE_DIR "${THIRD_PARTY_DIR}/glog/src/src/windows")
set(GLOG_INCLUDE_DIR "${GLOG_WINDOWS_INCLUDE_DIR}")

set(LIBOGG_INCLUDE_DIR "${THIRD_PARTY_DIR}/libogg")
set(LIBOGG_LIB_DIR "${LIBOGG_INCLUDE_DIR}/${LIB_SUB_DIR}")
# TODO(tomfinegan): Windows only, correct for other platforms.
set(LIBOGG_LIB_NAME "libogg_static.lib")

set(LIBVORBIS_INCLUDE_DIR "${THIRD_PARTY_DIR}/libvorbis")
set(LIBVORBIS_LIB_DIR "${LIBVORBIS_INCLUDE_DIR}/${LIB_SUB_DIR}")
# TODO(tomfinegan): Windows only, correct for other platforms.
set(LIBVORBIS_LIB_NAME "libvorbis_static.lib")

set(LIBVPX_INCLUDE_DIR "${THIRD_PARTY_DIR}/libvpx")
set(LIBVPX_LIB_DIR "${LIBVPX_INCLUDE_DIR}/${LIB_SUB_DIR}")

# TODO(tomfinegan): Windows only, correct for other platforms.
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(LIBVPX_LIB_NAME "vpxmtd.lib")
else(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(LIBVPX_LIB_NAME "vpxmt.lib")
endif(CMAKE_BUILD_TYPE STREQUAL "Debug")

set(LIBWEBM_INCLUDE_DIR "${THIRD_PARTY_DIR}")
set(LIBWEBM_LIB_DIR "${LIBWEBM_INCLUDE_DIR}/libwebm/${LIB_SUB_DIR}")
# TODO(tomfinegan): Windows only, correct for other platforms.
set(LIBWEBM_LIB_NAME "libwebm.lib")

set(LIBYUV_INCLUDE_DIR "${THIRD_PARTY_DIR}/libyuv/include")
set(LIBYUV_LIB_DIR "${LIBYUV_INCLUDE_DIR}/../${LIB_SUB_DIR}")
# TODO(tomfinegan): Windows only, correct for other platforms.
set(LIBYUV_LIB_NAME "yuv.lib")


#
# Add dependencies (on cmake projects within webmlive and third party libs).
#
add_subdirectory("${THIRD_PARTY_DIR}/directshow"
                 "${CMAKE_CURRENT_BINARY_DIR}/directshow")
add_subdirectory("${THIRD_PARTY_DIR}/glog"
                 "${CMAKE_CURRENT_BINARY_DIR}/glog")

add_executable(encoder
               audio_encoder.cc
               audio_encoder.h
               basictypes.h
               buffer_pool-inl.h
               buffer_pool.h
               buffer_util.cc
               buffer_util.h
               data_sink.h
               encoder_base.h
               encoder_main.cc
               http_uploader.cc
               http_uploader.h
               video_encoder.cc
               video_encoder.h
               vorbis_encoder.cc
               vorbis_encoder.h
               vpx_encoder.cc
               vpx_encoder.h
               webm_buffer_parser.cc
               webm_buffer_parser.h
               webm_encoder.cc
               webm_encoder.h
               webm_mux.cc
               webm_mux.h)
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/.."
                    "${CURL_INCLUDE_DIR}"
                    "${GLOG_INCLUDE_DIR}"
                    "${LIBOGG_INCLUDE_DIR}"
                    "${LIBVORBIS_INCLUDE_DIR}"
                    "${LIBVPX_INCLUDE_DIR}"
                    "${LIBWEBM_INCLUDE_DIR}"
                    "${LIBYUV_INCLUDE_DIR}")

target_link_libraries(encoder google-glog)

if(WIN32)
  set(WEBMDSHOW_INCLUDE_DIR "${THIRD_PARTY_DIR}/webmdshow")
  add_library(encoder_win STATIC
              win/audio_sink_filter.cc
              win/audio_sink_filter.h
              win/dshow_util.cc
              win/dshow_util.h
              win/media_source_dshow.cc
              win/media_source_dshow.h
              win/media_type_dshow.cc
              win/media_type_dshow.h
              win/video_sink_filter.cc
              win/video_sink_filter.h
              win/webm_guids.cc
              win/webm_guids.h)
  include_directories("${CMAKE_CURRENT_SOURCE_DIR}/win")
  target_include_directories(encoder_win SYSTEM PUBLIC
                             "${DSHOW_INCLUDE_DIR}"
                             "${DSHOW_INCLUDE_DIR}/baseclasses"
                             "${WEBMDSHOW_INCLUDE_DIR}")
  # Link with webmlive cmake libs and windows libs.
  target_link_libraries(encoder
                        encoder_win
                        dshow_baseclasses
                        quartz
                        shlwapi
                        strmiids
                        winmm
                        ws2_32)
  # Add complete path and lib name in order to link third party libs.
  target_link_libraries(encoder
                        "${CURL_LIB_DIR}/${CURL_LIB_NAME}"
                        "${LIBOGG_LIB_DIR}/${LIBOGG_LIB_NAME}"
                        "${LIBVORBIS_LIB_DIR}/${LIBVORBIS_LIB_NAME}"
                        "${LIBVPX_LIB_DIR}/${LIBVPX_LIB_NAME}"
                        "${LIBWEBM_LIB_DIR}/${LIBWEBM_LIB_NAME}"
                        "${LIBYUV_LIB_DIR}/${LIBYUV_LIB_NAME}")
endif(WIN32)

