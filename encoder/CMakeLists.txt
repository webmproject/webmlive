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
  add_definitions("/wd4996 /DWIN32_LEAN_AND_MEAN")

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

set(LIB_SUB_DIR "${LIB_OS_NAME}/${LIB_TARGET_NAME}")

#
# Build include and library paths.
#
set(THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third_party")

set(LIBCURL_INCLUDE_DIR "${THIRD_PARTY_DIR}/libcurl/include")
set(CURLBUILD_INCLUDE_DIR
    "${LIBCURL_INCLUDE_DIR}/curl/${LIB_OS_NAME}/${LIB_TARGET_NAME}")
  set(LIBCURL_LIB_DIR "${THIRD_PARTY_DIR}/libcurl/${LIB_SUB_DIR}")
if(WIN32)
  add_definitions("/DCURL_STATICLIB")
endif(WIN32)
# TODO(tomfinegan): Windows only, correct for other platforms.
set(LIBCURL_LIB_NAME "libcurl.lib")
set(LIBCURL_DBG_LIB "${LIBCURL_LIB_DIR}/debug/${LIBCURL_LIB_NAME}")
set(LIBCURL_REL_LIB "${LIBCURL_LIB_DIR}/release/${LIBCURL_LIB_NAME}")

set(DSHOW_INCLUDE_DIR "${THIRD_PARTY_DIR}/directshow")

# TODO(tomfinegan): Windows only, correct for other platforms.
set(GLOG_WINDOWS_INCLUDE_DIR "${THIRD_PARTY_DIR}/glog/src/src/windows")
set(GLOG_INCLUDE_DIR "${GLOG_WINDOWS_INCLUDE_DIR}")

set(LIBOGG_INCLUDE_DIR "${THIRD_PARTY_DIR}/libogg")
set(LIBOGG_LIB_DIR "${LIBOGG_INCLUDE_DIR}/${LIB_SUB_DIR}")
# TODO(tomfinegan): Windows only, correct for other platforms.
set(LIBOGG_LIB_NAME "libogg_static.lib")
set(LIBOGG_DBG_LIB "${LIBOGG_LIB_DIR}/debug/${LIBOGG_LIB_NAME}")
set(LIBOGG_REL_LIB "${LIBOGG_LIB_DIR}/release/${LIBOGG_LIB_NAME}")

set(LIBVORBIS_INCLUDE_DIR "${THIRD_PARTY_DIR}/libvorbis")
set(LIBVORBIS_LIB_DIR "${LIBVORBIS_INCLUDE_DIR}/${LIB_SUB_DIR}")
# TODO(tomfinegan): Windows only, correct for other platforms.
set(LIBVORBIS_LIB_NAME "libvorbis_static.lib")
set(LIBVORBIS_DBG_LIB  "${LIBVORBIS_LIB_DIR}/debug/${LIBVORBIS_LIB_NAME}")
set(LIBVORBIS_REL_LIB  "${LIBVORBIS_LIB_DIR}/release/${LIBVORBIS_LIB_NAME}")

set(LIBVPX_INCLUDE_DIR "${THIRD_PARTY_DIR}/libvpx")
set(LIBVPX_LIB_DIR "${LIBVPX_INCLUDE_DIR}/${LIB_SUB_DIR}")
# TODO(tomfinegan): Windows only, correct for other platforms.
set(LIBVPX_DBG_LIB "${LIBVPX_LIB_DIR}/debug/vpxmtd.lib")
set(LIBVPX_REL_LIB "${LIBVPX_LIB_DIR}/release/vpxmt.lib")

set(LIBWEBM_INCLUDE_DIR "${THIRD_PARTY_DIR}")
set(LIBWEBM_LIB_DIR "${LIBWEBM_INCLUDE_DIR}/libwebm/${LIB_SUB_DIR}")
# TODO(tomfinegan): Windows only, correct for other platforms.
set(LIBWEBM_LIB_NAME "libwebm.lib")
set(LIBWEBM_DBG_LIB "${LIBWEBM_LIB_DIR}/debug/${LIBWEBM_LIB_NAME}")
set(LIBWEBM_REL_LIB "${LIBWEBM_LIB_DIR}/release/${LIBWEBM_LIB_NAME}")

set(LIBYUV_INCLUDE_DIR "${THIRD_PARTY_DIR}/libyuv/include")
set(LIBYUV_LIB_DIR "${LIBYUV_INCLUDE_DIR}/../${LIB_SUB_DIR}")
# TODO(tomfinegan): Windows only, correct for other platforms.
set(LIBYUV_LIB_NAME "yuv.lib")
set(LIBYUV_DBG_LIB "${LIBYUV_LIB_DIR}/debug/${LIBYUV_LIB_NAME}")
set(LIBYUV_REL_LIB "${LIBYUV_LIB_DIR}/release/${LIBYUV_LIB_NAME}")

#
# Add dependencies (on cmake projects within webmlive and third party libs).
#
add_subdirectory("${THIRD_PARTY_DIR}/directshow"
                 "${CMAKE_CURRENT_BINARY_DIR}/directshow")
add_subdirectory("${THIRD_PARTY_DIR}/glog"
                 "${CMAKE_CURRENT_BINARY_DIR}/glog")

#
# Create the encoder target.
#
add_executable(encoder
               audio_encoder.cc
               audio_encoder.h
               basictypes.h
               buffer_pool-inl.h
               buffer_pool.h
               buffer_util.cc
               buffer_util.h
               dash_writer.cc
               dash_writer.h
               data_sink.cc
               data_sink.h
               encoder_base.h
               encoder_main.cc
               file_writer.cc
               file_writer.h
               http_uploader.cc
               http_uploader.h
               time_util.cc
               time_util.h
               video_encoder.cc
               video_encoder.h
               vorbis_encoder.cc
               vorbis_encoder.h
               vpx_encoder.cc
               vpx_encoder.h
               webm_encoder.cc
               webm_encoder.h
               webm_mux.cc
               webm_mux.h)
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/.."
                    "${LIBCURL_INCLUDE_DIR}"
                    "${CURLBUILD_INCLUDE_DIR}"
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
              win/string_util_win.cc
              win/string_util_win.h
              win/video_sink_filter.cc
              win/video_sink_filter.h
              win/webm_guids.cc
              win/webm_guids.h)
  include_directories("${CMAKE_CURRENT_SOURCE_DIR}/win"
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
  # Add complete path to library for debug and release versions of third party
  # libraries.
  target_link_libraries(encoder
                        optimized "${LIBCURL_REL_LIB}"
                        debug "${LIBCURL_DBG_LIB}"
                        optimized "${LIBOGG_REL_LIB}"
                        debug "${LIBOGG_DBG_LIB}"
                        optimized "${LIBVORBIS_REL_LIB}"
                        debug "${LIBVORBIS_DBG_LIB}"
                        optimized "${LIBVPX_REL_LIB}"
                        debug "${LIBVPX_DBG_LIB}"
                        optimized "${LIBWEBM_REL_LIB}"
                        debug "${LIBWEBM_DBG_LIB}"
                        optimized "${LIBYUV_REL_LIB}"
                        debug "${LIBYUV_DBG_LIB}")
endif(WIN32)
