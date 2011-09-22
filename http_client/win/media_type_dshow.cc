// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "win/media_type_dshow.h"

#include <mmreg.h>

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

// Allocates base AM_MEDIA_TYPE storage. Used by specialized media type classes.
int MediaType::AllocTypeStruct() {
  const SIZE_T type_size = sizeof(AM_MEDIA_TYPE);
  ptr_type_ = reinterpret_cast<AM_MEDIA_TYPE*>(CoTaskMemAlloc(type_size));
  if (!ptr_type_) {
    LOG(ERROR) << "AM_MEDIA_TYPE CoTaskMemAlloc returned NULL!";
    return kNoMemory;
  }
  memset(ptr_type_, 0, type_size);
  return kSuccess;
}

// Allocates memory for AM_MEDIA_TYPE format blob.
int MediaType::AllocFormatBlob(SIZE_T blob_size) {
  if (blob_size == 0) {
    LOG(ERROR) << "cannot allocate format blob of size 0.";
    return kInvalidArg;
  }
  if (!ptr_type_) {
    LOG(ERROR) << "NULL media type.";
    return kNullType;
  }
  ptr_type_->pbFormat = reinterpret_cast<BYTE*>(CoTaskMemAlloc(blob_size));
  if (!ptr_type_->pbFormat) {
    LOG(ERROR) << "format blob CoTaskMemAlloc returned NULL!";
    return kNoMemory;
  }
  memset(ptr_type_->pbFormat, 0, blob_size);
  ptr_type_->cbFormat = blob_size;
  return kSuccess;
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
  if (AllocTypeStruct()) {
    LOG(ERROR) << "AllocTypeStruct failed.";
    return kNoMemory;
  }
  ptr_type_->majortype = major_type;
  ptr_type_->formattype = format_type;
  // Determine blob size for |format_type|.
  const SIZE_T blob_size = (format_type == FORMAT_VideoInfo) ?
      sizeof(VIDEOINFOHEADER) : sizeof(VIDEOINFOHEADER2);
  // Alloc storage for |format_type|'s format block.
  if (AllocFormatBlob(blob_size)) {
    LOG(ERROR) << "AllocFormatBlob failed.";
    return kNoMemory;
  }
  return kSuccess;
}

// Copies |media_type| data to |ptr_type_| using |Init| overload to allocate
// storage for |ptr_type_|.
int VideoMediaType::Init(const AM_MEDIA_TYPE& media_type) {
  const int status = Init(media_type.majortype, media_type.formattype);
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

// Inits |ptr_type_| with majortype MEDIATYPE_Video and formattype
// FORMAT_VideoInfo. Uses |Init(const GUID&, const GUID&)|.
int VideoMediaType::Init() {
  return Init(MEDIATYPE_Video, FORMAT_VideoInfo);
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
  if (config.frame_rate <= 0 && config.frame_rate != kDefaultVideoFrameRate) {
    LOG(ERROR) << "Invalid frame rate.";
    return kInvalidArg;
  }
  // Confirm that |sub_type| is supported.
  switch (sub_type) {
    case kI420:
    case kYV12:
    case kYUY2:
    case kYUYV:
      break;
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
          reinterpret_cast<const VIDEOINFOHEADER*>(ptr_type_->pbFormat);
      time_per_frame = ptr_header->AvgTimePerFrame;
    } else if (ptr_type_->formattype == FORMAT_VideoInfo2) {
      const VIDEOINFOHEADER* ptr_header =
          reinterpret_cast<const VIDEOINFOHEADER*>(ptr_type_->pbFormat);
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
    const VIDEOINFOHEADER* ptr_header =
        reinterpret_cast<const VIDEOINFOHEADER*>(ptr_type_->pbFormat);
    return &ptr_header->bmiHeader;
  } else if (ptr_type_->formattype == FORMAT_VideoInfo2) {
    const VIDEOINFOHEADER2* ptr_header =
        reinterpret_cast<const VIDEOINFOHEADER2*>(ptr_type_->pbFormat);
    return &ptr_header->bmiHeader;
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
  switch (sub_type) {
    case kI420:
      ptr_type_->subtype = MEDIASUBTYPE_I420;
      header.biCompression = MAKEFOURCC('I', '4', '2', '0');
      header.biBitCount = kI420BitCount;
      header.biPlanes = 3;
      break;
    case kYV12:
      ptr_type_->subtype = MEDIASUBTYPE_YV12;
      header.biCompression = MAKEFOURCC('Y', 'V', '1', '2');
      header.biBitCount = kYV12BitCount;
      header.biPlanes = 3;
      break;
    case kYUY2:
      ptr_type_->subtype = MEDIASUBTYPE_YUY2;
      header.biCompression = MAKEFOURCC('Y', 'U', 'Y', '2');
      header.biBitCount = kYUY2BitCount;
      header.biPlanes = 1;
      break;
    case kYUYV:
      ptr_type_->subtype = MEDIASUBTYPE_YUYV;
      header.biCompression = MAKEFOURCC('Y', 'U', 'Y', 'V');
      header.biBitCount = kYUYVBitCount;
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
  if (config.frame_rate != kDefaultVideoFrameRate) {
    // Set frame rate.
    ptr_header->AvgTimePerFrame =
        seconds_to_media_time(1.0 / config.frame_rate);
  }
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
  if (config.frame_rate != kDefaultVideoFrameRate) {
    // Set frame rate.
    ptr_header->AvgTimePerFrame =
        seconds_to_media_time(1.0 / config.frame_rate);
  }
  // Configure the BITMAPINFOHEADER.
  return ConfigureFormatInfo(config, sub_type, ptr_header->bmiHeader);
}

///////////////////////////////////////////////////////////////////////////////
// AudioMediaType
//
AudioMediaType::AudioMediaType() {
}

AudioMediaType::~AudioMediaType() {
}

// Allocates storage for AM_MEDIA_TYPE struct (|ptr_type_|), and its format
// blob (|ptr_type_->pbFormat|).
int AudioMediaType::Init(const GUID& major_type, const GUID& format_type) {
  FreeMediaType(ptr_type_);
  if (major_type != MEDIATYPE_Audio) {
    LOG(ERROR) << "Unsupported major type.";
    return kUnsupportedMajorType;
  }
  if (format_type != FORMAT_WaveFormatEx) {
    LOG(ERROR) << "Unsupported format type.";
    return kUnsupportedFormatType;
  }
  if (AllocTypeStruct()) {
    LOG(ERROR) << "AllocTypeStruct failed.";
    return kNoMemory;
  }
  ptr_type_->majortype = major_type;
  ptr_type_->formattype = format_type;
  ptr_type_->bFixedSizeSamples = TRUE;
  ptr_type_->subtype = MEDIASUBTYPE_PCM;
  const SIZE_T blob_size = sizeof(WAVEFORMATEX);
  // Alloc storage for |format_type|'s format block.
  if (AllocFormatBlob(blob_size)) {
    LOG(ERROR) << "AllocFormatBlob failed.";
    return kNoMemory;
  }
  WAVEFORMATEX* ptr_wave_format =
      reinterpret_cast<WAVEFORMATEX*>(ptr_type_->pbFormat);
  ptr_wave_format->wFormatTag = WAVE_FORMAT_PCM;
  return kSuccess;
}

// Copies |media_type| data to |ptr_type_|.
int AudioMediaType::Init(const AM_MEDIA_TYPE& media_type) {
  FreeMediaType(ptr_type_);
  if (media_type.majortype != MEDIATYPE_Audio) {
    LOG(ERROR) << "Unsupported major type.";
    return kUnsupportedMajorType;
  }
  if (media_type.formattype != FORMAT_WaveFormatEx) {
    LOG(ERROR) << "Unsupported format type.";
    return kUnsupportedFormatType;
  }
  if (AllocTypeStruct()) {
    LOG(ERROR) << "AllocTypeStruct failed.";
    return kNoMemory;
  }
  memcpy_s(ptr_type_, sizeof(AM_MEDIA_TYPE), &media_type,
           sizeof(AM_MEDIA_TYPE));
  if (AllocFormatBlob(media_type.cbFormat)) {
    LOG(ERROR) << "AllocFormatBlob failed.";
    return kNoMemory;
  }
  memcpy_s(ptr_type_->pbFormat, ptr_type_->cbFormat, media_type.pbFormat,
           media_type.cbFormat);
  return kSuccess;
}

// Calls |Init()| with major type MEDIATYPE_Audio and format type
// FORMAT_WaveFormatEx, and returns result.
int AudioMediaType::Init() {
  return Init(MEDIATYPE_Audio, FORMAT_WaveFormatEx);
}

// Configures AM_MEDIA_TYPE FORMAT_WaveFormatEx format blob using user settings
// stored in |config|. Supports only WAVE_FORMAT_PCM.
int AudioMediaType::Configure(const AudioConfig& config) {
  WAVEFORMATEX* ptr_wave_format =
      reinterpret_cast<WAVEFORMATEX*>(ptr_type_->pbFormat);
  if (!ptr_wave_format) {
    LOG(ERROR) << "NULL audio format blob.";
    return kUnsupportedFormatType;
  }
  if (ptr_wave_format->wFormatTag != WAVE_FORMAT_PCM) {
    LOG(ERROR) << "Types other than WAVE_FORMAT_PCM are unsupported.";
    return kUnsupportedFormatType;
  }
  ptr_wave_format->nChannels = static_cast<WORD>(config.channels);
  ptr_wave_format->nSamplesPerSec = config.sample_rate;
  ptr_wave_format->wBitsPerSample = static_cast<uint16>(config.sample_size);
  const int bytes_per_sample = (ptr_wave_format->wBitsPerSample + 7) / 8;
  ptr_wave_format->nAvgBytesPerSec =
      config.channels * config.sample_rate * bytes_per_sample;
  ptr_wave_format->nBlockAlign =
      static_cast<WORD>(bytes_per_sample * config.channels);
  ptr_type_->lSampleSize = ptr_wave_format->nBlockAlign;
  return kSuccess;
}

// Returns number of channels.
int AudioMediaType::channels() const {
  if (!ptr_type_ || !ptr_type_->pbFormat) {
    LOG(ERROR) << "no type or no format blob, can't read channel count.";
    return 0;
  }
  if (ptr_type_->cbFormat < sizeof(WAVEFORMATEX)) {
    LOG(ERROR) << "audio format blob is too small, can't read channel count.";
    return 0;
  }
  const WAVEFORMATEX* ptr_wave_format =
      reinterpret_cast<const WAVEFORMATEX*>(ptr_type_->pbFormat);
  return ptr_wave_format->nChannels;
}

// Returns sample rate in samples per second.
int AudioMediaType::sample_rate() const {
  if (!ptr_type_ || !ptr_type_->pbFormat) {
    LOG(ERROR) << "no type or no format blob, can't read sample rate.";
    return 0;
  }
  if (ptr_type_->cbFormat < sizeof(WAVEFORMATEX)) {
    LOG(ERROR) << "audio format blob is too small, can't read sample rate.";
    return 0;
  }
  const WAVEFORMATEX* ptr_wave_format =
      reinterpret_cast<const WAVEFORMATEX*>(ptr_type_->pbFormat);
  return ptr_wave_format->nSamplesPerSec;
}

// Returns sample size in bits.
int AudioMediaType::sample_size() const {
  if (!ptr_type_ || !ptr_type_->pbFormat) {
    LOG(ERROR) << "no type or no format blob, can't read sample size.";
    return 0;
  }
  if (ptr_type_->cbFormat < sizeof(WAVEFORMATEX)) {
    LOG(ERROR) << "audio format blob is too small, can't read sample size.";
    return 0;
  }
  const WAVEFORMATEX* ptr_wave_format =
      reinterpret_cast<const WAVEFORMATEX*>(ptr_type_->pbFormat);
  return ptr_wave_format->wBitsPerSample;
}

///////////////////////////////////////////////////////////////////////////////
// MediaTypePtr
//
MediaTypePtr::MediaTypePtr() : ptr_type_(NULL) {
}

MediaTypePtr::MediaTypePtr(AM_MEDIA_TYPE* ptr_type): ptr_type_(ptr_type) {
}

MediaTypePtr::~MediaTypePtr() {
  MediaType::FreeMediaType(ptr_type_);
}

// Releases |ptr_type_| and takes ownership of |ptr_type|. Rreturns |kSuccess|,
// or |kNullType| if |ptr_type| is NULL.
int MediaTypePtr::Attach(AM_MEDIA_TYPE* ptr_type) {
  if (!ptr_type) {
    LOG(ERROR) << "NULL media type.";
    return kNullType;
  }
  Free();
  ptr_type_ = ptr_type;
  return kSuccess;
}

void MediaTypePtr::Free() {
  MediaType::FreeMediaType(ptr_type_);
  ptr_type_ = NULL;
}

// Copies |ptr_type_| and sets it to NULL, then returns the copy.
AM_MEDIA_TYPE* MediaTypePtr::Detach() {
  AM_MEDIA_TYPE* ptr_type = ptr_type_;
  ptr_type_ = NULL;
  return ptr_type;
}

// Frees and returns pointer to |ptr_type_|.
AM_MEDIA_TYPE** MediaTypePtr::operator&() {  // NOLINT
  Free();
  return &ptr_type_;
}

}  // namespace webmlive
