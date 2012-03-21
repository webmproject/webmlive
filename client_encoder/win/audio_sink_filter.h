// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
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
#ifndef CLIENT_ENCODER_WIN_AUDIO_SINK_FILTER_H_
#define CLIENT_ENCODER_WIN_AUDIO_SINK_FILTER_H_

#pragma warning(push)
#pragma warning(disable:4005)
// Disable C4005 via pragma
// Pragma required because streams.h includes intsafe.h, which defines
// INTSAFE_E_ARITHMETIC_OVERFLOW without an ifndef check, resulting in a
// warning that breaks our build when compiling with warnings-as-errors flag
// enabled.
#include "baseclasses/streams.h"
#pragma warning(pop)
#include "boost/scoped_array.hpp"
#include "boost/scoped_ptr.hpp"
#include "client_encoder/basictypes.h"
#include "client_encoder/client_encoder_base.h"
#include "client_encoder/webm_encoder.h"

namespace webmlive {

// Forward declare |AudioSinkFilter| for use in |AudioSinkPin|.
class AudioSinkFilter;

// Pin class used by |AudioSinkFilter|.
class AudioSinkPin : public CBaseInputPin {
 public:
  const static int kNumInputSubTypes = 2;
  const static GUID kInputSubTypes[kNumInputSubTypes];
  // Constructs CBaseInputPin and returns result via |ptr_result|. Returns
  // S_OK when successful.
  AudioSinkPin(TCHAR* ptr_object_name,
               AudioSinkFilter* ptr_filter,
               CCritSec* ptr_filter_lock,
               HRESULT* ptr_result,
               LPCWSTR ptr_pin_name);
  virtual ~AudioSinkPin();

  //
  // CBaseInputPin methods
  //

  // Stores preferred media type for |type_index| in |ptr_media_type|. Supports
  //
  // Return values:
  // S_OK - success, |type_index| in range and |ptr_media_type| written.
  // VFW_S_NO_MORE_ITEMS - |type_index| != 0.
  // E_OUTOFMEMORY - could not allocate format buffer.
  HRESULT GetMediaType(int32 type_index, CMediaType* ptr_media_type);

  // Checks if AM_MEDIA_TYPE stored in CMediaType pointer is acceptable.
  // Supports only
  // Return values:
  // S_OK - |ptr_media_type| is supported.
  // E_INVALIDARG - NULL |ptr_media_type|.
  // VFW_E_TYPE_NOT_ACCEPTED - |ptr_media_type| is not supported.
  HRESULT CheckMediaType(const CMediaType* ptr_media_type);

  //
  // IPin method(s).
  //

  // Receives audio buffers from the upstream filter and passes them to
  // |AudioSinkFilter::OnSamplesReceived|.
  // Returns S_OK, or the HRESULT error value returned CBaseInputPin::Receive
  // if it fails.
  STDMETHODIMP Receive(IMediaSample* ptr_sample);

 private:
  // Copies |actual_config_| to |ptr_config| and returns S_OK. Returns
  // E_POINTER when |ptr_config| is NULL.
  HRESULT config(AudioConfig* ptr_config);

  // Resets |actual_config_| and copies |config| values to |requested_config_|,
  // then returns S_OK.
  HRESULT set_config(const AudioConfig& config);

  // Filter user's requested video config.
  AudioConfig requested_config_;

  // Actual video config (from upstream filter).
  AudioConfig actual_config_;

  WEBMLIVE_DISALLOW_COPY_AND_ASSIGN(AudioSinkPin);
  friend class AudioSinkFilter;
};

// Video sink filter class. Receives I420 video frames from upstream
// DirectShow filter via |AudioSinkPin|.
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

  // Copies actual video configuration to |ptr_config| and returns S_OK.
  HRESULT config(AudioConfig* ptr_config);

  // Sets actual requested video configuration and returns S_OK.
  HRESULT set_config(const AudioConfig& config);

  // IUnknown
  DECLARE_IUNKNOWN;

  // CBaseFilter methods
  int GetPinCount() { return 1; }

  // Returns the pin at |index|, or NULL. The value of |index| must be 0.
  CBasePin* GetPin(int index);

 private:
  // Copes audio samples from |ptr_sample|
  // |AudioSamplesCallbackInterface| for processing.
  // Returns S_OK when successful.
  HRESULT OnSamplesReceived(IMediaSample* ptr_sample);
  CCritSec filter_lock_;
  boost::scoped_ptr<AudioSinkPin> sink_pin_;
  AudioSamplesCallbackInterface* ptr_samples_callback_;
  WEBMLIVE_DISALLOW_COPY_AND_ASSIGN(AudioSinkFilter);
  friend class AudioSinkPin;
};

}  // namespace webmlive

#endif  // CLIENT_ENCODER_WIN_AUDIO_SINK_FILTER_H_
