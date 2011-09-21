// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef HTTP_CLIENT_WIN_MEDIA_TYPE_DSHOW_H_
#define HTTP_CLIENT_WIN_MEDIA_TYPE_DSHOW_H_

// files included by dshow.h cause many 4996 warnings, disable them:
#pragma warning(disable:4996)
#include <dshow.h>
#pragma warning(default:4996)
#include <dvdmedia.h>

#include "basictypes.h"
#include "webm_encoder.h"

namespace webmlive {

// Base class for audio and video AM_MEDIA_TYPE management classes.
class MediaType {
 public:
  enum {
    kNullType = -7,  // |ptr_type_| NULL.
    kInvalidArg = -6,
    kUnsupportedSubType = -5,
    kNotImplemented = -4,
    kNoMemory = -3,
    kUnsupportedFormatType = -2,
    kUnsupportedMajorType = -1,
    kSuccess = 0,
  };
  MediaType();
  virtual ~MediaType();
  // Returns |ptr_type_|.
  const AM_MEDIA_TYPE* get() const;
  virtual int Init(const GUID& major_type, const GUID& format_type) = 0;
  virtual int Init(const AM_MEDIA_TYPE& media_type) = 0;
  // Utility functions for free'ing |AM_MEDIA_TYPE| pointers.
  static void FreeMediaType(AM_MEDIA_TYPE* ptr_media_type);
  static void FreeMediaTypeData(AM_MEDIA_TYPE* ptr_media_type);
 private:
  // Pointer to AM_MEDIA_TYPE memory allocated via CoTaskMemAlloc.
  AM_MEDIA_TYPE* ptr_type_;
  friend class VideoMediaType;
  WEBMLIVE_DISALLOW_COPY_AND_ASSIGN(MediaType);
};

// Video specific AM_MEDIA_TYPE management class. Always allocates internal
// storage for the AM_MEDIA_TYPE data.
class VideoMediaType : public MediaType {
 public:
  static const int kI420BitCount = 12;
  static const int kIYUVBitCount = kI420BitCount;
  static const int kYV12BitCount = kI420BitCount;
  static const int kYUY2BitCount = 16;
  static const int kYUYVBitCount = kYUY2BitCount;
  static const int kUYVYBitCount = kYUY2BitCount;
  typedef WebmEncoderConfig::VideoCaptureConfig VideoConfig;
  enum VideoSubType {
    kI420 = 0,
    kIYUV = 1,  // same as I420, just a different type name and 4cc.
    kYV12 = 2,
    kYUY2 = 3,
    kYUYV = 4,  // same as YUY2, just a different type name and 4cc.
    kUYVY = 5,
  };

  VideoMediaType();
  virtual ~VideoMediaType();
  // Allocates AM_MEDIA_TYPE for specified |major_type| and |format_type|.
  // Supports only |major_type| equal to MEDIATYPE_Video, and |format_type|'s
  // FORMAT_VideoInfo and FORMAT_VideoInfo2. Returns |kSuccess| for supported
  // types.
  virtual int Init(const GUID& major_type, const GUID& format_type);
  // Copies |media_type| and returns |kSuccess|.
  virtual int Init(const AM_MEDIA_TYPE& media_type);
  // Configures format block using |sub_type| and |config|. Directly applies
  // settings specified by |config| and returns success for supported
  // |sub_type| values. Note that not all |VideoSubType| values are supported.
  int ConfigureSubType(VideoSubType sub_type, const VideoConfig& config);
  // Mutators that allow direct control of frame rate without touching the
  // BITMAPINFOHEADER.
  int set_avg_time_per_frame(REFERENCE_TIME time_per_frame);
  int set_frame_rate(double frame_rate);
  // Accessors for frame rate, time per frame, width, and height.
  double frame_rate() const;
  REFERENCE_TIME avg_time_per_frame() const;
  int width() const;
  int height() const;
 private:
  // Easy access helper for obtaining values from the BITMAPINFOHEADER within
  // |ptr_type_|'s format blob.
  const BITMAPINFOHEADER* bitmap_header() const;
  // Configures |header| for specified |subtype| using values from |config|.
  int ConfigureFormatInfo(const VideoConfig& config, VideoSubType sub_type,
                          BITMAPINFOHEADER& header);
  // Sets source and target rectangles to respect |config.width| and
  // |config.height|.
  void ConfigureRects(const VideoConfig& config, RECT& source, RECT& target);
  // Sets VIDEOINFOHEADER members within pbFormat block of AM_MEDIA_TYPE.
  int ConfigureVideoInfoHeader(const VideoConfig& config,
                               VideoSubType sub_type,
                               VIDEOINFOHEADER* header);
  // Sets VIDEOINFOHEADER2 members within pbFormat block of AM_MEDIA_TYPE.
  int ConfigureVideoInfoHeader2(const VideoConfig& config,
                                VideoSubType sub_type,
                                VIDEOINFOHEADER2* header);
  WEBMLIVE_DISALLOW_COPY_AND_ASSIGN(VideoMediaType);
};

}  // namespace webmlive

#endif  // HTTP_CLIENT_WIN_MEDIA_TYPE_DSHOW_H_
