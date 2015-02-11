// Copyright (c) 2012 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
//
// Defines the video sink filter used to obtain raw frames from user input
// devices available via DirectShow. Based on WebRTC's CaptureInputPin and
// CaptureSinkFilter.
#ifndef ENCODER_WIN_AUDIO_SINK_FILTER_H_
#define ENCODER_WIN_AUDIO_SINK_FILTER_H_

// Wrap include of streams.h with include guard used in the file: including the
// file twice results in the output "STREAMS.H included TWICE" for debug
// builds.
#ifndef __STREAMS__
#pragma warning(push)
#pragma warning(disable:4005)
// Disable C4005 via pragma
// Pragma required because streams.h includes intsafe.h, which defines
// INTSAFE_E_ARITHMETIC_OVERFLOW without an ifndef check, resulting in a
// warning that breaks our build when compiling with warnings-as-errors flag
// enabled.
#include "baseclasses/streams.h"
#pragma warning(pop)
#endif  // __STREAMS__
#include "boost/scoped_array.hpp"
#include "boost/scoped_ptr.hpp"
#include "encoder/audio_encoder.h"
#include "encoder/basictypes.h"
#include "encoder/encoder_base.h"


namespace webmlive {

// Forward declare |AudioSinkFilter| for use in |AudioSinkPin|.
class AudioSinkFilter;

// Pin class used by |AudioSinkFilter|.
class AudioSinkPin : public CBaseInputPin {
 public:
  static const int kNumInputSubTypes = 2;
  static const GUID kInputSubTypes[kNumInputSubTypes];
  // Constructs CBaseInputPin and returns result via |ptr_result|. Returns
  // S_OK when successful.
  AudioSinkPin(TCHAR* ptr_object_name,
               AudioSinkFilter* ptr_filter,
               CCritSec* ptr_filter_lock,
               HRESULT* ptr_result,
               LPCWSTR ptr_pin_name);
  virtual ~AudioSinkPin();

  //
  // CBasePin methods
  //

  // Stores preferred media type for |type_index| in |ptr_media_type|. Supports
  // only MEDIASUBTYPE_IEEE_FLOAT and MEDIASUBTYPE_PCM.
  // Return values:
  // S_OK - success, |type_index| in range and |ptr_media_type| written.
  // VFW_S_NO_MORE_ITEMS - |type_index| != 0.
  // E_OUTOFMEMORY - could not allocate format buffer.
  virtual HRESULT GetMediaType(int32 type_index, CMediaType* ptr_media_type);

  // Checks if AM_MEDIA_TYPE stored in CMediaType pointer is acceptable.
  // Supports only
  // Return values:
  // S_OK - |ptr_media_type| is supported.
  // E_INVALIDARG - NULL |ptr_media_type|.
  // VFW_E_TYPE_NOT_ACCEPTED - |ptr_media_type| is not supported.
  virtual HRESULT CheckMediaType(const CMediaType* ptr_media_type);

  //
  // IMemInputPin method(s).
  //

  // Receives audio buffers from the upstream filter and passes them to
  // |AudioSinkFilter::OnSamplesReceived|.
  // Returns S_OK, or the HRESULT error value returned CBaseInputPin::Receive
  // if it fails.
  virtual HRESULT STDMETHODCALLTYPE Receive(IMediaSample* ptr_sample);

 private:
  // Copies |actual_config_| to |ptr_config| and returns S_OK. Returns
  // E_POINTER when |ptr_config| is NULL.
  HRESULT config(AudioConfig* ptr_config) const;

  // Resets |actual_config_| and copies |config| values to |requested_config_|,
  // then returns S_OK.
  HRESULT set_config(const AudioConfig& config);

  // Filter user's requested audio config.
  AudioConfig requested_config_;

  // Actual audio config (from upstream filter).
  AudioConfig actual_config_;

  WEBMLIVE_DISALLOW_COPY_AND_ASSIGN(AudioSinkPin);
  // |AudioSinkFilter| requires access to private member |actual_config_|, and
  // private methods |config| and |set_config| for configuration retrieval and
  // control.
  friend class AudioSinkFilter;
};

// Audio sink filter class. Receives IEEE floating point or PCM samples from
// upstream DirectShow filter via |AudioSinkPin|.
class AudioSinkFilter : public CBaseFilter {
 public:
  // Stores |ptr_samples_callback|, constructs CBaseFilter and |AudioSinkPin|,
  // and returns result via |ptr_result|.
  // Return values:
  // S_OK - success.
  // E_INVALIDARG - |ptr_samples_callback| is NULL.
  // E_OUTOFMEMORY - cannot construct |sink_pin_|.
  AudioSinkFilter(const TCHAR* ptr_filter_name,
                  LPUNKNOWN ptr_iunknown,
                  AudioSamplesCallbackInterface* ptr_samples_callback,
                  HRESULT* ptr_result);
  virtual ~AudioSinkFilter();

  // Copies actual audio configuration to |ptr_config| and returns S_OK.
  HRESULT config(AudioConfig* ptr_config) const;

  // Sets requested audio configuration and returns S_OK.
  HRESULT set_config(const AudioConfig& config);

  // IUnknown
  DECLARE_IUNKNOWN;

  // CBaseFilter methods
  virtual int GetPinCount() { return 1; }

  // Returns the pin at |index|, or NULL. The value of |index| must be 0.
  virtual CBasePin* GetPin(int index);

 private:
  // Copies audio samples from |ptr_sample| to |sample_buffer_|, and passes
  // |sample_buffer_| to |AudioSamplesCallbackInterface| for processing.
  // Returns S_OK when successful.
  HRESULT OnSamplesReceived(IMediaSample* ptr_sample);

  mutable CCritSec filter_lock_;
  boost::scoped_ptr<AudioSinkPin> sink_pin_;
  AudioBuffer sample_buffer_;
  AudioSamplesCallbackInterface* ptr_samples_callback_;
  WEBMLIVE_DISALLOW_COPY_AND_ASSIGN(AudioSinkFilter);

  // |AudioSinkPin| requires access to |AudioSinkFilter| private member
  // |filter_lock_|, and private method |OnSamplesReceived| to lock the filter
  // and safely deliver sample buffers.
  friend class AudioSinkPin;
};

}  // namespace webmlive

#endif  // ENCODER_WIN_AUDIO_SINK_FILTER_H_
