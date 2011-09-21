// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "win/media_type_dshow.h"

#include "glog/logging.h"
#include "win/webm_encoder_dshow.h"
#include "win/webm_guids.h"

namespace webmlive {

///////////////////////////////////////////////////////////////////////////////
// MediaType
//
MediaType::MediaType(): ptr_type_(NULL) {
}

MediaType::~MediaType() {
  FreeMediaType(ptr_type_);
}

const AM_MEDIA_TYPE* MediaType::get() const {
  return ptr_type_;
}

// Utility function for cleaning up AM_MEDIA_TYPE and its format blob.
void MediaType::FreeMediaType(AM_MEDIA_TYPE* ptr_media_type) {
  if (ptr_media_type) {
    FreeMediaTypeData(ptr_media_type);
    CoTaskMemFree(ptr_media_type);
  }
}

// Utility function for proper clean up of AM_MEDIA_TYPE format blob.
void MediaType::FreeMediaTypeData(AM_MEDIA_TYPE* ptr_media_type) {
  if (ptr_media_type) {
    if (ptr_media_type->cbFormat != 0) {
      CoTaskMemFree(ptr_media_type->pbFormat);
      ptr_media_type->cbFormat = 0;
      ptr_media_type->pbFormat = NULL;
    }
    if (ptr_media_type->pUnk != NULL) {
      // |pUnk| should not be used, but because the Microsoft example code
      // has this section, it's included here. (The example also includes the
      // note, "pUnk should not be used", but does not explain why cleanup code
      // remains in place).
      ptr_media_type->pUnk->Release();
      ptr_media_type->pUnk = NULL;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// VideoMediaType
//
VideoMediaType::VideoMediaType() {
}

VideoMediaType::~VideoMediaType() {
}

// Allocates storage for AM_MEDIA_TYPE struct (|ptr_type_|), and its format
// blob (|ptr_type_->pbFormat|).
int VideoMediaType::Init(const GUID& major_type, const GUID& format_type) {
  FreeMediaType(ptr_type_);
  if (major_type != MEDIATYPE_Video) {
    LOG(ERROR) << "Unsupported major type.";
    return kUnsupportedMajorType;
  }
  if (format_type != FORMAT_VideoInfo && format_type != FORMAT_VideoInfo2) {
    LOG(ERROR) << "Unsupported format type.";
    return kUnsupportedFormatType;
  }
  // Allocate basic |AM_MEDIA_TYPE| storage, and assign |ptr_type_|.
  const uint64 type_size = sizeof(AM_MEDIA_TYPE);
  ptr_type_ = static_cast<AM_MEDIA_TYPE*>(CoTaskMemAlloc(type_size));
  if (!ptr_type_) {
    LOG(ERROR) << "AM_MEDIA_TYPE CoTaskMemAlloc returned NULL!";
    return kNoMemory;
  }
  memset(ptr_type_, 0, sizeof(AM_MEDIA_TYPE));
  ptr_type_->majortype = major_type;
  ptr_type_->formattype = format_type;
  // Store size of |format_type|.
  ptr_type_->cbFormat = format_type == FORMAT_VideoInfo ?
      sizeof(VIDEOINFOHEADER) : sizeof(VIDEOINFOHEADER2);
  // Alloc storage for |format_type|'s format block.
  ptr_type_->pbFormat =
      static_cast<BYTE*>(CoTaskMemAlloc(ptr_type_->cbFormat));
  if (!ptr_type_->pbFormat) {
    LOG(ERROR) << "AM_MEDIA_TYPE format blob CoTaskMemAlloc returned NULL!";
    return kNoMemory;
  }
  memset(ptr_type_, 0, ptr_type_->cbFormat);
  return kSuccess;
}

// Copies |media_type| data to |ptr_type_| using |Init| overload to allocate
// storage for |ptr_type_|.
int VideoMediaType::Init(const AM_MEDIA_TYPE& media_type) {
  int status = Init(media_type.majortype, media_type.formattype);
  if (status) {
    LOG(ERROR) << "Init failed, status=" << status;
    return status;
  }
  *ptr_type_ = media_type;
  // TODO(tomfinegan): It might be better to allow override of cbFormat via
  //                   an additional argument to |Init|.
  if (ptr_type_->cbFormat != media_type.cbFormat) {
    LOG(ERROR) << "AM_MEDIA_TYPE format size mismatch, expected="
               << ptr_type_->cbFormat << " actual=" << media_type.cbFormat
               << ".";
    return kUnsupportedFormatType;
  }
  memcpy(ptr_type_->pbFormat, media_type.pbFormat, ptr_type_->cbFormat);
  return kSuccess;
}

// Configures AM_MEDIA_TYPE format blob for given |sub_type| and |config|.
int VideoMediaType::ConfigureSubType(VideoSubType sub_type,
                                     const VideoConfig &config) {
  // Make sure configuration is sane.
  if (config.width <= 0) {
    LOG(ERROR) << "Invalid width.";
    return kInvalidArg;
  }
  if (config.height <= 0) {
    LOG(ERROR) << "Invalid height.";
    return kInvalidArg;
  }
  if (config.frame_rate <= 0) {
    LOG(ERROR) << "Invalid frame rate.";
    return kInvalidArg;
  }
  // Confirm that |sub_type| is supported.
  switch (sub_type) {
    case kI420:
    case kYV12:
    case kYUY2:
    case kUYVY:
      break;
    case kYUYV:
      // TODO(tomfinegan): Add YUYV support to the VP8 encode filter. It's
      //                   trivial: same format as YUY2, but a different 4cc.
      LOG(ERROR) << "the VP8 encoder filter does not support IYUV.";
      return kNotImplemented;
    case kIYUV:
      // TODO(tomfinegan): Add IYUV support to the VP8 encode filter. It's
      //                   trivial: same format as I420, but a different 4cc.
      LOG(ERROR) << "the VP8 encoder filter does not support IYUV.";
      return kNotImplemented;
    default:
      LOG(ERROR) << sub_type << " is not a known VideoSubType.";
      return kUnsupportedSubType;
  }
  int status = kUnsupportedSubType;
  if (ptr_type_->formattype == FORMAT_VideoInfo) {
    VIDEOINFOHEADER* ptr_header =
        reinterpret_cast<VIDEOINFOHEADER*>(ptr_type_->pbFormat);
    status = ConfigureVideoInfoHeader(config, sub_type, ptr_header);
  } else if (ptr_type_->formattype ==FORMAT_VideoInfo2) {
    VIDEOINFOHEADER2* ptr_header =
        reinterpret_cast<VIDEOINFOHEADER2*>(ptr_type_->pbFormat);
    status = ConfigureVideoInfoHeader2(config, sub_type, ptr_header);
  } else {
    LOG(ERROR) << "Unsupported format type.";
    status = kUnsupportedFormatType;
  }
  return status;
}

// Converts |frame_rate| to time per frame, and uses |set_avg_time_per_frame()|
// to apply frame rate to video info header's AvgTimePerFrame member.
int VideoMediaType::set_frame_rate(double frame_rate) {
  if (frame_rate <= 0) {
    LOG(ERROR) << "invalid frame_rate.";
    return kInvalidArg;
  }
  return set_avg_time_per_frame(seconds_to_media_time(1.0 / frame_rate));
}

// Sets video info header's AvgTimePerFrame member.
int VideoMediaType::set_avg_time_per_frame(REFERENCE_TIME time_per_frame) {
  if (time_per_frame <= 0) {
    LOG(ERROR) << "invalid time_per_frame.";
    return kInvalidArg;
  }
  if (!ptr_type_) {
    LOG(ERROR) << "Null ptr_type_.";
    return kNullType;
  }
  if (ptr_type_->formattype == FORMAT_VideoInfo) {
    VIDEOINFOHEADER* ptr_header =
        reinterpret_cast<VIDEOINFOHEADER*>(ptr_type_->pbFormat);
    ptr_header->AvgTimePerFrame = time_per_frame;
  } else if (ptr_type_->formattype == FORMAT_VideoInfo2) {
    VIDEOINFOHEADER* ptr_header =
        reinterpret_cast<VIDEOINFOHEADER*>(ptr_type_->pbFormat);
    ptr_header->AvgTimePerFrame = time_per_frame;
  }
  return kSuccess;
}

// Calculates frame rate from |avg_time_per_frame()|, and returns it.
double VideoMediaType::frame_rate() const {
  double frame_rate = 0.0;
  const REFERENCE_TIME time_per_frame = avg_time_per_frame();
  if (time_per_frame > 0) {
    frame_rate = 1.0 / media_time_to_seconds(time_per_frame);
  }
  return frame_rate;
}

// Returns time per frame in 100ns ticks.
REFERENCE_TIME VideoMediaType::avg_time_per_frame() const {
  REFERENCE_TIME time_per_frame = 0;
  if (ptr_type_) {
    if (ptr_type_->formattype == FORMAT_VideoInfo) {
      const VIDEOINFOHEADER* ptr_header =
          reinterpret_cast<VIDEOINFOHEADER*>(ptr_type_->pbFormat);
      time_per_frame = ptr_header->AvgTimePerFrame;
    } else if (ptr_type_->formattype == FORMAT_VideoInfo2) {
      const VIDEOINFOHEADER* ptr_header =
          reinterpret_cast<VIDEOINFOHEADER*>(ptr_type_->pbFormat);
      time_per_frame = ptr_header->AvgTimePerFrame;
    }
  }
  return time_per_frame;
}

// Returns biWidth value from BITMAPINFOHEADER.
int VideoMediaType::width() const {
  const BITMAPINFOHEADER* ptr_header = bitmap_header();
  int width = 0;
  if (ptr_header) {
    width = ptr_header->biWidth;
  }
  return width;
}

// Returns biHeight value from BITMAPINFOHEADER.
int VideoMediaType::height() const {
  const BITMAPINFOHEADER* ptr_header = bitmap_header();
  int height = 0;
  if (ptr_header) {
    height = ptr_header->biHeight;
  }
  return height;
}

// Returns pointer to BITMAPINFOHEADER stored within |ptr_type_|'s format
// blob.
const BITMAPINFOHEADER* VideoMediaType::bitmap_header() const {
  if (!ptr_type_ || !ptr_type_->pbFormat) {
    LOG(ERROR) << "Null ptr_type_ or format block.";
    return NULL;
  }
  if (ptr_type_->formattype == FORMAT_VideoInfo) {
    return &reinterpret_cast<VIDEOINFOHEADER*>(ptr_type_->pbFormat)->bmiHeader;
  } else if (ptr_type_->formattype == FORMAT_VideoInfo2) {
    return &reinterpret_cast<VIDEOINFOHEADER2*>(
        ptr_type_->pbFormat)->bmiHeader;
  }
  return NULL;
}

// Sets AM_MEDIA_TYPE subtype value, and configures BITMAPINFOHEADER using
// values calculated from |sub_type| and |config| entries.
int VideoMediaType::ConfigureFormatInfo(const VideoConfig& config,
                                        VideoSubType sub_type,
                                        BITMAPINFOHEADER& header) {
  header.biHeight = config.height;
  header.biWidth = config.width;
  switch(sub_type) {
    case kI420:
      ptr_type_->subtype = MEDIASUBTYPE_I420;
      header.biCompression = MAKEFOURCC('I','4','2','0');
      header.biBitCount = kI420BitCount;
      header.biPlanes = 3;
      break;
    case kYV12:
      ptr_type_->subtype = MEDIASUBTYPE_YV12;
      header.biCompression = MAKEFOURCC('Y','V','1','2');
      header.biBitCount = kYV12BitCount;
      header.biPlanes = 3;
      break;
    case kYUY2:
      ptr_type_->subtype = MEDIASUBTYPE_YUY2;
      header.biCompression = MAKEFOURCC('Y','U','Y','2');
      header.biBitCount = kYUY2BitCount;
      header.biPlanes = 1;
      break;
    case kUYVY:
      ptr_type_->subtype = MEDIASUBTYPE_UYVY;
      header.biCompression = MAKEFOURCC('U','Y','V','Y');
      header.biBitCount = kUYVYBitCount;
      header.biPlanes = 1;
      break;
    default:
      return kUnsupportedSubType;
  }
  header.biSizeImage = DIBSIZE(header);
  return kSuccess;
}

// Zeroes |source| and |target| origin coordinates, sets rectangle right
// value to |config.width| and bottom value to |config.height| to produce
// a video rectangle matching the user's settings.
void VideoMediaType::ConfigureRects(const VideoConfig& config,
                                    RECT& source, RECT& target) {
  source.top = source.left = 0;
  source.bottom = config.height;
  source.right = config.width;
  target.top = target.left = 0;
  target.bottom = config.height;
  target.right = config.width;
}

// Sets video rectangles, frame rate, and returns result of BITMAPINFOHEADER
// configuration attempt.
int VideoMediaType::ConfigureVideoInfoHeader(
    const VideoConfig& config,
    VideoSubType sub_type,
    VIDEOINFOHEADER* ptr_header) {
  // Set source and target rectangles.
  ConfigureRects(config, ptr_header->rcSource, ptr_header->rcTarget);
  // Set frame rate.
  ptr_header->AvgTimePerFrame = seconds_to_media_time(1.0 / config.frame_rate);
  // Configure BITMAPINFOHEADER.
  return ConfigureFormatInfo(config, sub_type, ptr_header->bmiHeader);
}

// Sets video rectangles, frame rate, and returns result of BITMAPINFOHEADER
// configuration attempt.
int VideoMediaType::ConfigureVideoInfoHeader2(
    const VideoConfig& config,
    VideoSubType sub_type,
    VIDEOINFOHEADER2* ptr_header) {
  // Set source and target rectangles.
  ConfigureRects(config, ptr_header->rcSource, ptr_header->rcTarget);
  // Set frame rate.
  ptr_header->AvgTimePerFrame = seconds_to_media_time(1.0 / config.frame_rate);
  // Configure the BITMAPINFOHEADER.
  return ConfigureFormatInfo(config, sub_type, ptr_header->bmiHeader);
}

}  // namespace webmlive
