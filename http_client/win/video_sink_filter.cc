// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "http_client/win/video_sink_filter.h"

#include <dvdmedia.h>
#include <vfwmsgs.h>

#include "glog/logging.h"
#include "http_client/win/media_source_dshow.h"
#include "http_client/win/webm_guids.h"
#include "webmdshow/common/hrtext.hpp"
#include "webmdshow/common/odbgstream.hpp"

// Extracts error from the HRESULT, and outputs its hex and decimal values.
#define HRLOG(X) \
    " {" << #X << "=" << X << "/" << std::hex << X << std::dec << " (" << \
    hrtext(X) << ")}"

// TODO(tomfinegan): webrtc uses baseclasses, but has worked around the need
//                   for the next two lines. Determining how to do so would be
//                   enlightening, but isn't that important.
//                   Without these two lines dllentry.cpp from the baseclasses
//                   sources will cause an error at link time (LNK2001,
//                   unresolved external symbol) because of use of the following
//                   two globals via extern.
CFactoryTemplate* g_Templates = NULL;   // NOLINT
int g_cTemplates = 0;                   // NOLINT

namespace webmlive {

///////////////////////////////////////////////////////////////////////////////
// VideoSinkPin
//

VideoSinkPin::VideoSinkPin(TCHAR* ptr_object_name,
                           VideoSinkFilter* ptr_filter,
                           CCritSec* ptr_filter_lock,
                           HRESULT* ptr_result,
                           LPCWSTR ptr_pin_name)
    : CBaseInputPin(ptr_object_name, ptr_filter, ptr_filter_lock, ptr_result,
                    ptr_pin_name),
      stride_(0) {
}

VideoSinkPin::~VideoSinkPin() {
}

// Returns preferred media type.
HRESULT VideoSinkPin::GetMediaType(int32 type_index,
                                   CMediaType* ptr_media_type) {
  // TODO: add libyuv and support types other than I420
  if (type_index != 0) {
    return VFW_S_NO_MORE_ITEMS;
  }
  VIDEOINFOHEADER* const ptr_video_info =
      reinterpret_cast<VIDEOINFOHEADER*>(
          ptr_media_type->AllocFormatBuffer(sizeof(VIDEOINFOHEADER)));
  if (!ptr_video_info) {
    LOG(ERROR) << "VIDEOINFOHEADER alloc failed.";
    return E_OUTOFMEMORY;
  }
  ZeroMemory(ptr_video_info, sizeof(VIDEOINFOHEADER));
  ptr_video_info->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);

  // Use empty source/dest rectangle-- the entire image is needed, and there is
  // no target subrect.
  SetRectEmpty(&ptr_video_info->rcSource);
  SetRectEmpty(&ptr_video_info->rcTarget);

  // Set values for all input types supported.
  ptr_media_type->SetType(&MEDIATYPE_Video);
  ptr_media_type->SetFormatType(&FORMAT_VideoInfo);
  ptr_media_type->SetTemporalCompression(FALSE);
  ptr_video_info->bmiHeader.biWidth = requested_config_.width;
  ptr_video_info->bmiHeader.biHeight = requested_config_.height;

  // Set sub type and format data for I420.
  ptr_video_info->bmiHeader.biCompression = MAKEFOURCC('I','4','2','0');
  const int kI420BitCount = 12;
  ptr_video_info->bmiHeader.biBitCount = kI420BitCount;
  ptr_video_info->bmiHeader.biPlanes = 1;
  ptr_media_type->SetSubtype(&MEDIASUBTYPE_I420);

  // Set sample size.
  ptr_video_info->bmiHeader.biSizeImage = DIBSIZE(ptr_video_info->bmiHeader);
  ptr_media_type->SetSampleSize(ptr_video_info->bmiHeader.biSizeImage);
  LOG(INFO) << "\n GetMediaType type_index=" << type_index << "\n"
            << "   width=" << requested_config_.width << "\n"
            << "   height=" << requested_config_.height << "\n"
            << std::hex << "   biCompression="
            << ptr_video_info->bmiHeader.biCompression;
  return S_OK;
}

// Confirms that |ptr_media_type| is VIDEOINFOHEADER or VIDEOINFOHEADER2 and
// has a subtype of MEDIASUBTYPE_I420.
HRESULT VideoSinkPin::CheckMediaType(const CMediaType* ptr_media_type) {
  // Confirm media type is acceptable.
  const GUID* const ptr_type_guid = ptr_media_type->Type();
  if (!ptr_type_guid || *ptr_type_guid != MEDIATYPE_Video) {
    return VFW_E_TYPE_NOT_ACCEPTED;
  }

  // Confirm that subtype and formattype GUIDs can be obtained.
  const GUID* const ptr_subtype_guid = ptr_media_type->Subtype();
  const GUID* const ptr_format_guid = ptr_media_type->FormatType();
  if (!ptr_subtype_guid || !ptr_format_guid) {
      return E_INVALIDARG;
  }

  // Inspect the format stored in |ptr_media_type|.
  const GUID& format_guid = *ptr_format_guid;
  const GUID& subtype_guid = *ptr_subtype_guid;
  if (format_guid == FORMAT_VideoInfo) {
    const VIDEOINFOHEADER* const ptr_video_info =
        reinterpret_cast<VIDEOINFOHEADER*>(ptr_media_type->Format());
    if (!ptr_video_info) {
      return VFW_E_TYPE_NOT_ACCEPTED;
    }
    const DWORD& four_cc = ptr_video_info->bmiHeader.biCompression;
    if (subtype_guid != MEDIASUBTYPE_I420 ||
        four_cc != MAKEFOURCC('I','4','2','0')) {
      return VFW_E_TYPE_NOT_ACCEPTED;
    }

    // Store current format in |actual_config_|; |CBasePin::ReceiveConnection|
    // always calls |CheckMediaType|.
    actual_config_.width = ptr_video_info->bmiHeader.biWidth;
    actual_config_.height = abs(ptr_video_info->bmiHeader.biHeight);
  } else if (format_guid == FORMAT_VideoInfo2) {
    const VIDEOINFOHEADER2* const ptr_video_info =
        reinterpret_cast<VIDEOINFOHEADER2*>(ptr_media_type->Format());
    if (!ptr_video_info) {
      return VFW_E_TYPE_NOT_ACCEPTED;
    }
    const DWORD& four_cc = ptr_video_info->bmiHeader.biCompression;
    if (subtype_guid != MEDIASUBTYPE_I420 ||
        four_cc != MAKEFOURCC('I','4','2','0')) {
      return VFW_E_TYPE_NOT_ACCEPTED;
    }

    // Store current format in |actual_config_|; |CBasePin::ReceiveConnection|
    // always calls |CheckMediaType|.
    actual_config_.width = ptr_video_info->bmiHeader.biWidth;
    actual_config_.height = abs(ptr_video_info->bmiHeader.biHeight);

    // Store the stride for use with |VideoFrame::Init()|-- it's needed for
    // format conversion.
    stride_ = DIBWIDTHBYTES(ptr_video_info->bmiHeader);
  }
  LOG(INFO) << "\n CheckMediaType actual settings\n"
            << "   width=" << requested_config_.width << "\n"
            << "   height=" << requested_config_.height << "\n"
            << "   stride=" << stride_;
  return S_OK;
}

// Calls CBaseInputPin::Receive and then passes |ptr_sample| to
// |VideoSinkFilter::OnFrameReceived|.
HRESULT VideoSinkPin::Receive(IMediaSample* ptr_sample) {
  CHECK_NOTNULL(m_pFilter);
  CHECK_NOTNULL(ptr_sample);
  VideoSinkFilter* ptr_filter = reinterpret_cast <VideoSinkFilter*>(m_pFilter);
  CAutoLock lock(&ptr_filter->filter_lock_);
  HRESULT hr = CBaseInputPin::Receive(ptr_sample);
  if (FAILED(hr)) {
    if (hr != VFW_E_WRONG_STATE) {
      // Log the error only when it is not |VFW_E_WRONG_STATE|. The filter
      // graph appears to always call |Receive()| once after |Stop()|.
      LOG(ERROR) << "CBaseInputPin::Receive failed. " << HRLOG(hr);
    }
    return hr;
  }
  hr = ptr_filter->OnFrameReceived(ptr_sample);
  if (FAILED(hr)) {
    LOG(ERROR) << "OnFrameReceived failed. " << HRLOG(hr);
  }
  return S_OK;
}

// Copies |actual_config_| to |ptr_config|. Note that the filter lock is always
// held by caller, |VideoSinkFilter::config|.
HRESULT VideoSinkPin::config(VideoConfig* ptr_config) {
  if (!ptr_config) {
    return E_POINTER;
  }
  *ptr_config = actual_config_;
  return S_OK;
}

// Sets |requested_config_| and resets |actual_config_|. Filter lock always
// held by caller, |VideoSinkFilter::set_config|.
HRESULT VideoSinkPin::set_config(const VideoConfig& config) {
  requested_config_ = config;
  actual_config_ = WebmEncoderConfig::VideoCaptureConfig();
  return S_OK;
}

///////////////////////////////////////////////////////////////////////////////
// VideoSinkFilter
//
VideoSinkFilter::VideoSinkFilter(
    const TCHAR* ptr_filter_name,
    LPUNKNOWN ptr_iunknown,
    VideoFrameCallbackInterface* ptr_frame_callback,
    HRESULT* ptr_result)
    : CBaseFilter(ptr_filter_name,
                  ptr_iunknown,
                  &filter_lock_,
                  CLSID_VideoSinkFilter) {
  if (!ptr_frame_callback) {
    *ptr_result = E_INVALIDARG;
    return;
  }
  ptr_frame_callback_ = ptr_frame_callback;
  sink_pin_.reset(
      new (std::nothrow) VideoSinkPin(NAME("VideoSinkInputPin"),  // NOLINT
                                      this, &filter_lock_, ptr_result,
                                      L"VideoSink"));
  if (!sink_pin_) {
    *ptr_result = E_OUTOFMEMORY;
  } else {
    *ptr_result = S_OK;
  }
}

VideoSinkFilter::~VideoSinkFilter() {
}

// Locks filter and returns |VideoSinkPin::config|.
HRESULT VideoSinkFilter::config(VideoConfig* ptr_config) {
  CAutoLock lock(&filter_lock_);
  return sink_pin_->config(ptr_config);
}

// Locks filter and returns |VideoSinkPin::set_config|.
HRESULT VideoSinkFilter::set_config(const VideoConfig& config) {
  if (m_State != State_Stopped) {
    return VFW_E_NOT_STOPPED;
  }
  CAutoLock lock(&filter_lock_);
  return sink_pin_->set_config(config);
}

// Locks filter and returns VideoSinkPin pointer wrapped by |sink_pin_|.
CBasePin* VideoSinkFilter::GetPin(int index) {
  CBasePin* ptr_pin = NULL;
  CAutoLock lock(&filter_lock_);
  if (index == 0) {
    ptr_pin = sink_pin_.get();
  }
  return ptr_pin;
}

// Lock owned by |VideoSinkPin::Receive|. Copies buffer from |ptr_sample| into
// |frame_|, and then passes |frame_| to
// |VideoFrameCallbackInterface::OnVideoFrameReceived|.
HRESULT VideoSinkFilter::OnFrameReceived(IMediaSample* ptr_sample) {
  if (!ptr_sample) {
    return E_POINTER;
  }
  BYTE* ptr_sample_buffer = NULL;
  HRESULT hr = ptr_sample->GetPointer(&ptr_sample_buffer);
  if (FAILED(hr) || !ptr_sample_buffer) {
    LOG(ERROR) << "OnFrameReceived called with empty sample.";
    hr = (hr == S_OK) ? E_FAIL : hr;
    return hr;
  }
  int64 timestamp = 0;
  int64 duration = 0;
  REFERENCE_TIME start_time = 0;
  REFERENCE_TIME end_time = 0;
  hr = ptr_sample->GetTime(&start_time, &end_time);
  if (FAILED(hr)) {
    LOG(WARNING) << "OnFrameReceived cannot get media time(s).";
  } else {
    timestamp = media_time_to_milliseconds(start_time);
    if (hr != VFW_S_NO_STOP_TIME) {
      duration = media_time_to_milliseconds(end_time) - timestamp;
    } else {
      LOG(WARNING) << "OnFrameReceived frame has no stop time.";
    }
  }
  const int32 width = sink_pin_->actual_config_.width;
  const int32 height = sink_pin_->actual_config_.height;
  // TODO(tomfinegan): Write an allocator that retrieves frames from
  //                   |WebmEncoder::EncoderThread| and avoid this extra copy.
  const int32 status = frame_.Init(kVideoFormatI420,
                                   true,  // all I420 frames are "keyframes"
                                   width, height, timestamp, duration,
                                   ptr_sample_buffer,
                                   ptr_sample->GetActualDataLength());
  if (status) {
    LOG(ERROR) << "OnFrameReceived frame init failed: " << status;
    return E_FAIL;
  }
  LOG(INFO) << "OnFrameReceived received a frame:"
            << " width=" << width << " height=" << height
            << " timestamp(sec)=" << (timestamp / 1000.0)
            << " timestamp=" << timestamp
            << " duration(sec)= " << (duration / 1000.0)
            << " duration= " << duration
            << " size=" << frame_.buffer_length();
  int frame_status = ptr_frame_callback_->OnVideoFrameReceived(&frame_);
  if (frame_status && frame_status != VideoFrameCallbackInterface::kDropped) {
    LOG(ERROR) << "OnVideoFrameReceived failed, status=" << frame_status;
  }
  return S_OK;
}

}  // namespace webmlive
