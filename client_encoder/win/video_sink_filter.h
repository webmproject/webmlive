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
#ifndef CLIENT_ENCODER_WIN_VIDEO_SINK_FILTER_H_
#define CLIENT_ENCODER_WIN_VIDEO_SINK_FILTER_H_

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
#include "client_encoder/basictypes.h"
#include "client_encoder/client_encoder_base.h"
#include "client_encoder/video_encoder.h"
#include "client_encoder/webm_encoder.h"

namespace webmlive {

// Forward declare |VideoSinkFilter| for use in |VideoSinkPin|.
class VideoSinkFilter;

// Pin class used by |VideoSinkFilter|. Accepts only I420 video input.
class VideoSinkPin : public CBaseInputPin {
 public:
  // Constructs CBaseInputPin and returns result via |ptr_result|. Returns
  // S_OK when successful.
  VideoSinkPin(TCHAR* ptr_object_name,
               VideoSinkFilter* ptr_filter,
               CCritSec* ptr_filter_lock,
               HRESULT* ptr_result,
               LPCWSTR ptr_pin_name);
  virtual ~VideoSinkPin();

  //
  // CBasePin methods
  //

  // Stores preferred media type for |type_index| in |ptr_media_type|. Supports
  // only a single type, I420.
  // Return values:
  // S_OK - success, |type_index| in range and |ptr_media_type| written.
  // VFW_S_NO_MORE_ITEMS - |type_index| != 0.
  // E_OUTOFMEMORY - could not allocate format buffer.
  virtual HRESULT GetMediaType(int32 type_index, CMediaType* ptr_media_type);

  // Checks if AM_MEDIA_TYPE stored in CMediaType pointer is acceptable.
  // Supports only MEDIASUBTYPE_I420 wrapped in VIDEOINFOHEADER or
  // VIDEOINFOHEADER2 structures.
  // Return values:
  // S_OK - |ptr_media_type| is supported.
  // E_INVALIDARG - NULL |ptr_media_type|.
  // VFW_E_TYPE_NOT_ACCEPTED - |ptr_media_type| is not supported.
  virtual HRESULT CheckMediaType(const CMediaType* ptr_media_type);

  //
  // IMemInputPin method(s).
  //

  // Receives video buffers from the upstream filter and passes them to
  // |VideoSinkFilter::OnFrameReceived|.
  // Returns S_OK, or the HRESULT error value returned CBaseInputPin::Receive
  // if it fails.
  virtual HRESULT STDMETHODCALLTYPE Receive(IMediaSample* ptr_sample);

 private:
  // Copies |actual_config_| to |ptr_config| and returns S_OK. Returns
  // E_POINTER when |ptr_config| is NULL.
  HRESULT config(VideoConfig* ptr_config);

  // Resets |actual_config_| and copies |config| values to |requested_config_|,
  // then returns S_OK.
  HRESULT set_config(const VideoConfig& config);

  // Returns true when |media_sub_type| is an acceptable video format.
  bool AcceptableSubType(const GUID& media_sub_type);

  // Filter user's requested video config.
  VideoConfig requested_config_;

  // Actual video config (from upstream filter).
  VideoConfig actual_config_;
  WEBMLIVE_DISALLOW_COPY_AND_ASSIGN(VideoSinkPin);

  // |VideoSinkFilter| requires access to private member |actual_config_|, and
  // private methods |config| and |set_config| for configuration retrieval and
  // control.
  friend class VideoSinkFilter;
};

// Video sink filter class. Receives I420 video frames from upstream
// DirectShow filter via |VideoSinkPin|.
class VideoSinkFilter : public CBaseFilter {
 public:
  // Stores |ptr_frame_callback|, constructs CBaseFilter and |VideoSinkPin, and
  // returns result via |ptr_result|.
  // Return values:
  // S_OK - success.
  // E_INVALIDARG - |ptr_Frame_callback| is NULL.
  // E_OUTOFMEMORY - cannot construct |sink_pin_|.
  VideoSinkFilter(const TCHAR* ptr_filter_name,
                  LPUNKNOWN ptr_iunknown,
                  VideoFrameCallbackInterface* ptr_frame_callback,
                  HRESULT* ptr_result);
  virtual ~VideoSinkFilter();

  // Copies actual video configuration to |ptr_config| and returns S_OK.
  HRESULT config(VideoConfig* ptr_config);

  // Sets actual requested video configuration and returns S_OK.
  HRESULT set_config(const VideoConfig& config);

  // IUnknown
  DECLARE_IUNKNOWN;

  // CBaseFilter methods
  virtual int GetPinCount() { return 1; }

  // Returns the pin at |index|, or NULL. The value of |index| must be 0.
  virtual CBasePin* GetPin(int index);

 private:
  // Copes video frame from |ptr_sample| to |frame_|, and passes |frame_| to
  // |VideoFrameCallbackInterface::OnVideoFrameReceived| for processing.
  // Returns S_OK when successful.
  HRESULT OnFrameReceived(IMediaSample* ptr_sample);
  CCritSec filter_lock_;
  VideoFrame frame_;
  boost::scoped_ptr<VideoSinkPin> sink_pin_;
  VideoFrameCallbackInterface* ptr_frame_callback_;
  WEBMLIVE_DISALLOW_COPY_AND_ASSIGN(VideoSinkFilter);

  // |VideoSinkPin| requires access to private member |filter_lock_|, and
  // private method |OnFrameReceived| to lock the filter and safely deliver
  // video frame buffers.
  friend class VideoSinkPin;
};

}  // namespace webmlive

#endif  // CLIENT_ENCODER_WIN_VIDEO_SINK_FILTER_H_
