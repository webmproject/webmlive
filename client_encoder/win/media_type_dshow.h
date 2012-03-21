// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef CLIENT_ENCODER_WIN_MEDIA_TYPE_DSHOW_H_
#define CLIENT_ENCODER_WIN_MEDIA_TYPE_DSHOW_H_

#include <dshow.h>
#include <dvdmedia.h>

#include "client_encoder/basictypes.h"
#include "client_encoder/webm_encoder.h"

namespace webmlive {

// Converts |format| to its associated AM_MEDIA_TYPE subtype field value,
// copies the subtype value to |ptr_sub_type|, and returns true. Always returns
// false when |ptr_sub_type| is NULL. Returns false for unknown |format|
// values.
bool VideoFormatToSubTypeGuid(VideoFormat format, GUID* ptr_sub_type);

// Base class for audio and video AM_MEDIA_TYPE management classes.
class MediaType {
 public:
  enum {
    kInvalidFormat = -9,  // Unsupported/nonsense format blob values.
    kUnsupportedFormatTag = -8,
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
  const AM_MEDIA_TYPE* get() const { return ptr_type_; }

  virtual int Init(const GUID& major_type, const GUID& format_type) = 0;
  virtual int Init(const AM_MEDIA_TYPE& media_type) = 0;
  virtual int Init() = 0;

  // Utility functions for free'ing |AM_MEDIA_TYPE| pointers.
  static void FreeMediaType(AM_MEDIA_TYPE* ptr_media_type);
  static void FreeMediaTypeData(AM_MEDIA_TYPE* ptr_media_type);

 protected:
  // Allocates memory for |ptr_type_|.
  int AllocTypeStruct();

  // Allocates memory for |ptr_type_->pbFormat|.
  int AllocFormatBlob(SIZE_T blob_size);

  // Pointer to AM_MEDIA_TYPE memory allocated via CoTaskMemAlloc.
  AM_MEDIA_TYPE* ptr_type_;
  WEBMLIVE_DISALLOW_COPY_AND_ASSIGN(MediaType);
};

// Video specific AM_MEDIA_TYPE management class. Always allocates internal
// storage for the AM_MEDIA_TYPE data.
class VideoMediaType : public MediaType {
 public:
  typedef WebmEncoderConfig::VideoConfig VideoConfig;
  VideoMediaType();
  virtual ~VideoMediaType();

  // Allocates AM_MEDIA_TYPE for specified |major_type| and |format_type|.
  // Supports only |major_type| equal to MEDIATYPE_Video, and |format_type|'s
  // FORMAT_VideoInfo and FORMAT_VideoInfo2. Returns |kSuccess| for supported
  // types.
  virtual int Init(const GUID& major_type, const GUID& format_type);

  // Copies |media_type| and returns |kSuccess|.
  virtual int Init(const AM_MEDIA_TYPE& media_type);

  // Allocates AM_MEDIA_TYPE with VIDEOINFOHEADER format blob, and sets
  // contents to 0.
  virtual int Init();

  // Sets |subtype| field in |ptr_type_| and returns |kSuccess|. Always returns
  // |kNullType| when |ptr_type_| is NULL.
  // Note: Calls |FreeMediaTypeData()| when format blob is non-NULL; this is
  //       required to make the AM_MEDIA_TYPE truly partial.
  int ConfigurePartialType(VideoFormat format);

  // Configures format block using |sub_type| and |config|. Directly applies
  // settings specified by |config| and returns success for supported
  // |sub_type| values. Note that not all |VideoSubType| values are supported.
  int ConfigureSubType(VideoFormat sub_type, const VideoConfig& config);

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
  int ConfigureFormatInfo(const VideoConfig& config, VideoFormat sub_type,
                          BITMAPINFOHEADER& header);

  // Sets source and target rectangles to respect |config.width| and
  // |config.height|.
  void ConfigureRects(const VideoConfig& config, RECT& source, RECT& target);

  // Sets VIDEOINFOHEADER members within pbFormat block of AM_MEDIA_TYPE.
  int ConfigureVideoInfoHeader(const VideoConfig& config,
                               VideoFormat sub_type,
                               VIDEOINFOHEADER* header);

  // Sets VIDEOINFOHEADER2 members within pbFormat block of AM_MEDIA_TYPE.
  int ConfigureVideoInfoHeader2(const VideoConfig& config,
                                VideoFormat sub_type,
                                VIDEOINFOHEADER2* header);
  WEBMLIVE_DISALLOW_COPY_AND_ASSIGN(VideoMediaType);
};

// Audio specific |AM_MEDIA_TYPE| management class. Always allocates internal
// storage for the |AM_MEDIA_TYPE| data.
class AudioMediaType : public MediaType {
 public:
  typedef WebmEncoderConfig::AudioConfig AudioConfig;
  AudioMediaType();
  virtual ~AudioMediaType();

  // Allocates |AM_MEDIA_TYPE| for specified |major_type| and |format_type|,
  // and returns |MediaType::kSuccess|.
  virtual int Init(const GUID& major_type, const GUID& format_type);

  // Copies |media_type| and returns |MediaType::kSuccess|.
  virtual int Init(const AM_MEDIA_TYPE& media_type);

  // Allocates |AM_MEDIA_TYPE| with |WAVEFORMATEX| format blob, sets contents
  // to 0, and returns |MediaType::kSuccess|.
  virtual int Init();

  // Validates |MediaType::ptr_type_| and verifies that format blob has
  // capacity for a |WAVEFORMATEX| struct. Returns true for valid blobs.
  bool IsValidWaveFormatExBlob() const;

  // Checks |ptr_type_| using |IsValidWaveFormatExBlob()|, and then confirms
  // the additional storage necessary for a |WAVEFORMATEXTENSIBLE| blob is
  // present. Returns true for valid blobs.
  bool IsValidWaveFormatExtensibleBlob() const;

  // Configures |ptr_type_| using format specified by |config|, and returns
  // |MediaType::kSuccess|.
  int Configure(const AudioConfig& config);

  // Accessors that reach into the |WAVEFORMATEX| stored within the format
  // blob. All return 0 when |IsValidWaveFormatExBlob()| returns false,
  // otherwise the value stored within the format blob is returned.
  uint16 block_align() const;
  uint32 bytes_per_second() const;
  uint16 channels() const;
  uint16 format_tag() const;
  uint32 sample_rate() const;
  uint16 bits_per_sample() const;

  // Accessors that reach into the |WAVEFORMATEXTENSIBLE| stored within the
  // format blob. All return 0 when |IsValidWaveFormatExBlob()| or
  // |IsValidWaveFormatExtensibleBlob()| return false, otherwise the value
  // stored within the format blob is returned.
  uint16 valid_bits_per_sample() const;
  uint16 samples_per_block() const;
  uint32 channel_mask() const;

  // Sub format |GUID| accessor; returns |GUID_NULL| when
  // |MediaType::ptr_type_| format blob is not |WAVEFORMATEXTENSIBLE|.
  GUID sub_format() const;

 private:
  WEBMLIVE_DISALLOW_COPY_AND_ASSIGN(AudioMediaType);
};

// Wrapper class for automatic disposal of AM_MEDIA_TYPE pointers.
class MediaTypePtr {
 public:
  enum {
    kNoMemory = -2,
    kInvalidArg = -1,
    kSuccess = 0,
  };
  MediaTypePtr();
  explicit MediaTypePtr(AM_MEDIA_TYPE* ptr_type);
  ~MediaTypePtr();

  // |Free()|s |ptr_type_| and takes ownership of |ptr_type|. Note that
  // |ptr_type| and the format blob within must be memory allocated via
  // CoTaskMemAlloc. Both pointers will be be passed to CoTaskMemFree in
  // |~MediaTypePtr()|.
  int Attach(AM_MEDIA_TYPE* ptr_type);

  // Allocates storage for a copy of |ptr_type|, copies contents of |ptr_type|,
  // and passes the resulting AM_MEDIA_TYPE to |Attach()|.
  int Copy(const AM_MEDIA_TYPE* ptr_type);

  // |Free()|s |ptr_type_| and sets it to NULL.
  void Free();

  // Returns |ptr_type_| and sets internal copy to NULL.
  AM_MEDIA_TYPE* Detach();

  // Returns |ptr_type_|.
  AM_MEDIA_TYPE* get() const { return ptr_type_; }

  // |Free()|s and returns pointer to |ptr_type_|.
  AM_MEDIA_TYPE** GetPtr();

 private:
  AM_MEDIA_TYPE* ptr_type_;
  WEBMLIVE_DISALLOW_COPY_AND_ASSIGN(MediaTypePtr);
};

}  // namespace webmlive

#endif  // CLIENT_ENCODER_WIN_MEDIA_TYPE_DSHOW_H_
