// Copyright (c) 2012 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "client_encoder/win/media_type_dshow.h"

#include <mmreg.h>

#include "client_encoder/win/media_source_dshow.h"
#include "client_encoder/win/webm_guids.h"
#include "glog/logging.h"

namespace webmlive {

bool VideoFormatToSubTypeGuid(VideoFormat format, GUID* ptr_sub_type) {
  bool converted = false;
  if (ptr_sub_type) {
    switch (format) {
      case kVideoFormatI420:
        *ptr_sub_type = MEDIASUBTYPE_I420;
        converted = true;
        break;
      case kVideoFormatVP8:
        *ptr_sub_type = MEDIASUBTYPE_VP80;
        converted = true;
        break;
      case kVideoFormatYV12:
        *ptr_sub_type = MEDIASUBTYPE_YV12;
        converted = true;
        break;
      case kVideoFormatYUY2:
        *ptr_sub_type = MEDIASUBTYPE_YUY2;
        converted = true;
        break;
      case kVideoFormatYUYV:
        *ptr_sub_type = MEDIASUBTYPE_YUYV;
        converted = true;
        break;
      case kVideoFormatUYVY:
        *ptr_sub_type = MEDIASUBTYPE_UYVY;
        converted = true;
        break;
      case kVideoFormatRGB:
        *ptr_sub_type = MEDIASUBTYPE_RGB24;
        converted = true;
        break;
      case kVideoFormatRGBA:
        *ptr_sub_type = MEDIASUBTYPE_RGB32;
        converted = true;
        break;
      default:
        LOG(WARNING) << "Unknown video format value.";
    }
  }
  return converted;
}

///////////////////////////////////////////////////////////////////////////////
// MediaType
//
MediaType::MediaType(): ptr_type_(NULL) {
}

MediaType::~MediaType() {
  FreeMediaType(ptr_type_);
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
      ptr_media_type->formattype = GUID_NULL;
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

int VideoMediaType::ConfigurePartialType(VideoFormat format) {
  if (!ptr_type_) {
    LOG(ERROR) << "internal AM_MEDIA_TYPE is NULL.";
    return kNullType;
  }
  if (ptr_type_->pbFormat) {
    // The |Init()| methods all allocate the format blob and set
    // |AM_MEDIA_TYPE::formattype|. Free the blob and set |formattype| to
    // GUID_NULL to make a truly partial media type.
    FreeMediaTypeData(ptr_type_);
  }
  int status = kInvalidArg;
  GUID subtype = GUID_NULL;
  if (VideoFormatToSubTypeGuid(format, &subtype)) {
    ptr_type_->subtype = subtype;
    status = kSuccess;
  }
  return status;
}

// Configures AM_MEDIA_TYPE format blob for given |sub_type| and |config|.
int VideoMediaType::ConfigureSubType(VideoFormat sub_type,
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

  // Confirm that |sub_type| is supported, and set the temporal compression
  // and fixed size samples fields in |ptr_type_|.
  switch (sub_type) {
    case kVideoFormatVP8:
      ptr_type_->bTemporalCompression = TRUE;
      ptr_type_->bFixedSizeSamples = FALSE;
      break;
    case kVideoFormatI420:
    case kVideoFormatYV12:
    case kVideoFormatYUY2:
    case kVideoFormatUYVY:
    case kVideoFormatRGB:
    case kVideoFormatRGBA:
      ptr_type_->bTemporalCompression = FALSE;
      ptr_type_->bFixedSizeSamples = TRUE;
      break;
    default:
      LOG(ERROR) << sub_type << " is not a known VideoFormat.";
      return kUnsupportedSubType;
  }

  // Configure the format blobs.
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
                                        VideoFormat sub_type,
                                        BITMAPINFOHEADER& header) {
  header.biSize = sizeof header;
  header.biHeight = config.height;
  header.biWidth = config.width;
  header.biPlanes = 1;
  switch (sub_type) {
    case kVideoFormatI420:
      ptr_type_->subtype = MEDIASUBTYPE_I420;
      header.biCompression = MAKEFOURCC('I', '4', '2', '0');
      header.biBitCount = kI420BitCount;
      break;
    case kVideoFormatVP8:
      ptr_type_->subtype = MEDIASUBTYPE_VP80;
      header.biCompression = MAKEFOURCC('V', 'P', '8', '0');
      header.biBitCount = 0;
      break;
    case kVideoFormatYV12:
      ptr_type_->subtype = MEDIASUBTYPE_YV12;
      header.biCompression = MAKEFOURCC('Y', 'V', '1', '2');
      header.biBitCount = kYV12BitCount;
      break;
    case kVideoFormatYUY2:
      ptr_type_->subtype = MEDIASUBTYPE_YUY2;
      header.biCompression = MAKEFOURCC('Y', 'U', 'Y', '2');
      header.biBitCount = kYUY2BitCount;
      break;
    case kVideoFormatYUYV:
      ptr_type_->subtype = MEDIASUBTYPE_YUYV;
      header.biCompression = MAKEFOURCC('Y', 'U', 'Y', 'V');
      header.biBitCount = kYUYVBitCount;
      break;
    case kVideoFormatUYVY:
      ptr_type_->subtype = MEDIASUBTYPE_UYVY;
      header.biCompression = MAKEFOURCC('U', 'Y', 'V', 'Y');
      header.biBitCount = kYUYVBitCount;
      break;
    case kVideoFormatRGB:
      ptr_type_->subtype = MEDIASUBTYPE_RGB24;
      header.biCompression = BI_RGB;
      header.biBitCount = kRGBBitCount;
      break;
    case kVideoFormatRGBA:
      ptr_type_->subtype = MEDIASUBTYPE_RGB32;
      header.biCompression = BI_RGB;
      header.biBitCount = kRGBABitCount;
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
    VideoFormat sub_type,
    VIDEOINFOHEADER* ptr_header) {
  // Set source and target rectangles.
  ConfigureRects(config, ptr_header->rcSource, ptr_header->rcTarget);
  if (config.frame_rate != 0) {
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
    VideoFormat sub_type,
    VIDEOINFOHEADER2* ptr_header) {
  // Set source and target rectangles.
  ConfigureRects(config, ptr_header->rcSource, ptr_header->rcTarget);
  if (config.frame_rate != 0) {
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
  WAVEFORMATEX* const ptr_wave_format =
      reinterpret_cast<WAVEFORMATEX* const>(ptr_type_->pbFormat);
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
  memcpy_s(ptr_type_, sizeof(*ptr_type_), &media_type, sizeof(media_type));
  if (AllocFormatBlob(media_type.cbFormat)) {
    LOG(ERROR) << "AllocFormatBlob failed.";
    return kNoMemory;
  }
  memcpy_s(ptr_type_->pbFormat, ptr_type_->cbFormat, media_type.pbFormat,
           media_type.cbFormat);
  return kSuccess;
}

int AudioMediaType::Init() {
  return Init(MEDIATYPE_Audio, FORMAT_WaveFormatEx);
}

bool AudioMediaType::IsValidWaveFormatExBlob() const {
  if (!ptr_type_) {
    LOG(ERROR) << "Invalid wave format: null media type.";
    return false;
  }
  if (!ptr_type_->pbFormat) {
    LOG(ERROR) << "Invalid wave format: null format blob.";
    return false;
  }
  if (ptr_type_->cbFormat < sizeof(WAVEFORMATEX)) {
    LOG(ERROR) << "Invalid wave format: format blob too small.";
    return false;
  }
  return true;
}

bool AudioMediaType::IsValidWaveFormatExtensibleBlob() const {
  if (!IsValidWaveFormatExBlob()) {
    LOG(ERROR) << "Invalid WAVEFORMATEXTENSIBLE: invalid WAVEFORMATEX.";
    return false;
  }
  if (ptr_type_->cbFormat < sizeof(WAVEFORMATEXTENSIBLE)) {
    LOG(ERROR) << "Invalid WAVEFORMATEXTENSIBLE: format blob too small.";
    return false;
  }
  if (format_tag() != WAVE_FORMAT_EXTENSIBLE) {
    LOG(ERROR) << "Invalid WAVEFORMATEXTENSIBLE: format tag incorrect.";
    return false;
  }
  return true;
}

int AudioMediaType::Configure(const AudioConfig& config) {
  if (!IsValidWaveFormatExBlob()) {
    LOG(ERROR) << "invalid format blob.";
    return kInvalidFormat;
  }

  WAVEFORMATEX* const ptr_wave_format =
      reinterpret_cast<WAVEFORMATEX*>(ptr_type_->pbFormat);
  if (!ptr_wave_format) {
    LOG(ERROR) << "NULL audio format blob.";
    return kUnsupportedFormatType;
  }
  if (ptr_wave_format->wFormatTag != WAVE_FORMAT_PCM &&
      ptr_wave_format->wFormatTag != WAVE_FORMAT_IEEE_FLOAT) {
    LOG(ERROR) << "cannot configure, internal type is not PCM or IEEE_FLOAT.";
    return kUnsupportedFormatType;
  }

  const int kBitsPerIeeeFloat = 32;
  if (ptr_wave_format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT &&
      config.bits_per_sample != kBitsPerIeeeFloat) {
    LOG(ERROR) << "cannot configure, sample size incorrect for IEEE_FLOAT.";
    return kInvalidFormat;
  }

  ptr_wave_format->nChannels = config.channels;
  ptr_wave_format->nSamplesPerSec = config.sample_rate;
  ptr_wave_format->wBitsPerSample = config.bits_per_sample;
  const int bytes_per_sample = (ptr_wave_format->wBitsPerSample + 7) / 8;
  ptr_wave_format->nAvgBytesPerSec =
      config.channels * config.sample_rate * bytes_per_sample;
  ptr_wave_format->nBlockAlign =
      static_cast<WORD>(bytes_per_sample * config.channels);
  ptr_type_->lSampleSize = ptr_wave_format->nBlockAlign;
  return kSuccess;
}

uint16 AudioMediaType::block_align() const {
  uint16 block_size = 0;
  if (IsValidWaveFormatExBlob()) {
    const WAVEFORMATEX* ptr_wave_format =
        reinterpret_cast<WAVEFORMATEX*>(ptr_type_->pbFormat);
    block_size = ptr_wave_format->nBlockAlign;
  }
  return block_size;
}

uint32 AudioMediaType::bytes_per_second() const {
  uint32 num_bytes_per_second = 0;
  if (IsValidWaveFormatExBlob()) {
    const WAVEFORMATEX* ptr_wave_format =
        reinterpret_cast<WAVEFORMATEX*>(ptr_type_->pbFormat);
    num_bytes_per_second = ptr_wave_format->nAvgBytesPerSec;
  }
  return num_bytes_per_second;
}

uint16 AudioMediaType::channels() const {
  uint16 num_channels = 0;
  if (IsValidWaveFormatExBlob()) {
    const WAVEFORMATEX* ptr_wave_format =
        reinterpret_cast<WAVEFORMATEX*>(ptr_type_->pbFormat);
    num_channels = ptr_wave_format->nChannels;
  }
  return num_channels;
}

uint16 AudioMediaType::format_tag() const {
  uint16 audio_format_tag = 0;
  if (IsValidWaveFormatExBlob()) {
    const WAVEFORMATEX* ptr_wave_format =
        reinterpret_cast<WAVEFORMATEX*>(ptr_type_->pbFormat);
    audio_format_tag = ptr_wave_format->wFormatTag;
  }
  return audio_format_tag;
}

uint32 AudioMediaType::sample_rate() const {
  uint32 samples_per_second = 0;
  if (IsValidWaveFormatExBlob()) {
    const WAVEFORMATEX* ptr_wave_format =
        reinterpret_cast<WAVEFORMATEX*>(ptr_type_->pbFormat);
    samples_per_second = ptr_wave_format->nSamplesPerSec;
  }
  return samples_per_second;
}

uint16 AudioMediaType::bits_per_sample() const {
  uint16 num_bits = 0;
  if (IsValidWaveFormatExBlob()) {
    const WAVEFORMATEX* ptr_wave_format =
        reinterpret_cast<WAVEFORMATEX*>(ptr_type_->pbFormat);
    num_bits = ptr_wave_format->wBitsPerSample;
  }
  return num_bits;
}

uint16 AudioMediaType::valid_bits_per_sample() const {
  uint16 num_valid_bits = 0;
  if (IsValidWaveFormatExtensibleBlob()) {
    const WAVEFORMATEXTENSIBLE* ptr_wave_format =
        reinterpret_cast<WAVEFORMATEXTENSIBLE*>(ptr_type_->pbFormat);
    num_valid_bits = ptr_wave_format->Samples.wValidBitsPerSample;
  }
  return num_valid_bits;
}

uint16 AudioMediaType::samples_per_block() const {
  uint16 num_samples = 0;
  if (IsValidWaveFormatExtensibleBlob()) {
    const WAVEFORMATEXTENSIBLE* ptr_wave_format =
        reinterpret_cast<WAVEFORMATEXTENSIBLE*>(ptr_type_->pbFormat);
    num_samples = ptr_wave_format->Samples.wSamplesPerBlock;
  }
  return num_samples;
}

uint32 AudioMediaType::channel_mask() const {
  uint32 channels_present = 0;
  if (IsValidWaveFormatExtensibleBlob()) {
    const WAVEFORMATEXTENSIBLE* ptr_wave_format =
        reinterpret_cast<WAVEFORMATEXTENSIBLE*>(ptr_type_->pbFormat);
    channels_present = ptr_wave_format->dwChannelMask;
  }
  return channels_present;
}

GUID AudioMediaType::sub_format() const {
  GUID audio_sub_format = GUID_NULL;
  if (IsValidWaveFormatExtensibleBlob()) {
    const WAVEFORMATEXTENSIBLE* ptr_wave_format =
        reinterpret_cast<WAVEFORMATEXTENSIBLE*>(ptr_type_->pbFormat);
    audio_sub_format = ptr_wave_format->SubFormat;
  }
  return audio_sub_format;
}

///////////////////////////////////////////////////////////////////////////////
// MediaTypePtr
//
MediaTypePtr::MediaTypePtr(): ptr_type_(NULL) {
}

MediaTypePtr::MediaTypePtr(AM_MEDIA_TYPE* ptr_type): ptr_type_(ptr_type) {
}

MediaTypePtr::~MediaTypePtr() {
  MediaType::FreeMediaType(ptr_type_);
}

// |Free()|s |ptr_type_| and takes ownership of |ptr_type|. Returns |kSuccess|,
// or |kNullType| if |ptr_type| is NULL.
int MediaTypePtr::Attach(AM_MEDIA_TYPE* ptr_type) {
  if (!ptr_type) {
    LOG(ERROR) << "NULL media type.";
    return kInvalidArg;
  }
  Free();
  ptr_type_ = ptr_type;
  return kSuccess;
}

int MediaTypePtr::Copy(const AM_MEDIA_TYPE* ptr_type) {
  if (!ptr_type) {
    LOG(ERROR) << "NULL media type.";
    return kInvalidArg;
  }
  AM_MEDIA_TYPE* const ptr_copy =
      reinterpret_cast<AM_MEDIA_TYPE*>(CoTaskMemAlloc(sizeof AM_MEDIA_TYPE));
  if (!ptr_copy) {
    LOG(ERROR) << "Cannot alloc media type.";
    return kNoMemory;
  }
  *ptr_copy = *ptr_type;
  if (ptr_type->pbFormat) {
    ptr_copy->pbFormat =
        reinterpret_cast<BYTE*>(CoTaskMemAlloc(ptr_type->cbFormat));
    if (!ptr_copy->pbFormat) {
      CoTaskMemFree(ptr_copy);
      LOG(ERROR) << "Cannot alloc format blob.";
      return kNoMemory;
    }
    memcpy(ptr_copy->pbFormat, ptr_type->pbFormat, ptr_type->cbFormat);
  }
  return Attach(ptr_copy);
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

// |Free()|s and returns pointer to |ptr_type_|.
AM_MEDIA_TYPE** MediaTypePtr::GetPtr() {
  Free();
  return &ptr_type_;
}

}  // namespace webmlive
