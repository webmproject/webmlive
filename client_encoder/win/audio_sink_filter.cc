// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "client_encoder/win/audio_sink_filter.h"

#include <dvdmedia.h>
#include <vfwmsgs.h>

#include "client_encoder/win/dshow_util.h"
#include "client_encoder/win/media_source_dshow.h"
#include "client_encoder/win/webm_guids.h"
#include "glog/logging.h"

namespace webmlive {

///////////////////////////////////////////////////////////////////////////////
// AudioSinkPin
//

AudioSinkPin::AudioSinkPin(TCHAR* ptr_object_name,
                           AudioSinkFilter* ptr_filter,
                           CCritSec* ptr_filter_lock,
                           HRESULT* ptr_result,
                           LPCWSTR ptr_pin_name)
    : CBaseInputPin(ptr_object_name, ptr_filter, ptr_filter_lock, ptr_result,
                    ptr_pin_name)
}

AudioSinkPin::~AudioSinkPin() {
}

// Returns preferred media type.
HRESULT AudioSinkPin::GetMediaType(int32 type_index,
                                   CMediaType* ptr_media_type) {
  if (type_index < 0 || !ptr_media_type) {
    return E_INVALIDARG;
  }
  if (type_index > 5) {
    return VFW_S_NO_MORE_ITEMS;
  }

  // TODO(tomfinegan): Must support these inputs:
  // MEDIATYPE_Audio/
  //  - FORMAT_WaveFormatEx/WAVE_FORMAT_IEE_FLOAT
  //  - FORMAT_WaveFormatEx/WAVE_FORMAT_PCM
  // The above provides mono/stereo support. For multi channel
  // |WAVEFORMATEXTENSIBLE| support is needed, which has some rules.
  // MSDN (http://goo.gl/tkK2y) says |WAVEFORMATEXTENSIBLE| must contain a
  // |WAVEFORMATEX| with |wFormatTag| set to |WAVE_FORMAT_EXTENSIBLE| and
  // |cbSize| >= 22.
  // Support of the following should be reasonable:
  //  - FORMAT_WaveFormatEx/WAVE_FORMAT_EXTENSIBLE/
  //      KSDATAFORMAT_SUBTYPE_IEEE_FLOAT
  //  - FORMAT_WaveFormatEx/WAVE_FORMAT_EXTENSIBLE/
  //      KSDATAFORMAT_SUBTYPE_PCM
  //  - FORMAT_WaveFormatEx/WAVE_FORMAT_EXTENSIBLE/
  //      KSDATAFORMAT_SUBTYPE_WAVEFORMATEX
  // One notable bit of fun: MSDN has conflicting examples for
  // WAVEFORMATEXTENSIBLE configuration. Driver stuff says

  return E_NOTIMPL;
}

HRESULT AudioSinkPin::CheckMediaType(const CMediaType* ptr_media_type) {
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

  const GUID& format_guid = *ptr_format_guid;
  const GUID& subtype_guid = *ptr_subtype_guid;

  // Confirm that the subtype is acceptable.
  if (!AcceptableSubType(subtype_guid)) {
    return VFW_E_TYPE_NOT_ACCEPTED;
  }

  // Obtain access to the BITMAPINFOHEADER, and confirm that the video format
  // is acceptable.
  const uint8* ptr_format = ptr_media_type->Format();
  const uint32 format_length = ptr_media_type->FormatLength();
  const BITMAPINFOHEADER* ptr_header =
      BitmapInfo(format_guid, ptr_format, format_length);
  if (ptr_header) {
    if (FourCCToVideoFormat(ptr_header->biCompression,
                            ptr_header->biBitCount,
                            &video_format_)) {
      // |ptr_media_type| contains a video frame with a pixel format that is
      // acceptable-- store format information.
      actual_config_.width = ptr_header->biWidth;
      actual_config_.height = ptr_header->biHeight;

      // Store the stride for use with |VideoFrame::Init()|-- it's needed for
      // format conversion.
      stride_ = DIBWIDTHBYTES(*ptr_header);
    }
  }

  LOG(INFO) << "\n CheckMediaType actual video settings\n"
            << "   width=" << actual_config_.width << "\n"
            << "   height=" << actual_config_.height << "\n"
            << "   stride=" << stride_ << "\n"
            << "   format=" << video_format_;
  return S_OK;
}

// Calls CBaseInputPin::Receive and then passes |ptr_sample| to
// |AudioSinkFilter::OnFrameReceived|.
HRESULT AudioSinkPin::Receive(IMediaSample* ptr_sample) {
  CHECK_NOTNULL(m_pFilter);
  CHECK_NOTNULL(ptr_sample);
  AudioSinkFilter* ptr_filter = reinterpret_cast<AudioSinkFilter*>(m_pFilter);
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

bool AudioSinkPin::AcceptableSubType(const GUID& media_sub_type) {
  return (media_sub_type == MEDIASUBTYPE_I420 ||
          media_sub_type == MEDIASUBTYPE_YV12 ||
          media_sub_type == MEDIASUBTYPE_YUY2 ||
          media_sub_type == MEDIASUBTYPE_YUYV ||
          media_sub_type == MEDIASUBTYPE_UYVY ||
          media_sub_type == MEDIASUBTYPE_RGB24 ||
          media_sub_type == MEDIASUBTYPE_RGB32);
}

// Copies |actual_config_| to |ptr_config|. Note that the filter lock is always
// held by caller, |VideoSinkFilter::config|.
HRESULT AudioSinkPin::config(VideoConfig* ptr_config) {
  if (!ptr_config) {
    return E_POINTER;
  }
  *ptr_config = actual_config_;
  return S_OK;
}

// Sets |requested_config_| and resets |actual_config_|. Filter lock always
// held by caller, |VideoSinkFilter::set_config|.
HRESULT AudioSinkPin::set_config(const VideoConfig& config) {
  requested_config_ = config;
  actual_config_ = WebmEncoderConfig::VideoCaptureConfig();
  return S_OK;
}

///////////////////////////////////////////////////////////////////////////////
// VideoSinkFilter
//
AudioSinkFilter::AudioSinkFilter(
    const TCHAR* ptr_filter_name,
    LPUNKNOWN ptr_iunknown,
    VideoFrameCallbackInterface* ptr_frame_callback,
    HRESULT* ptr_result)
    : CBaseFilter(ptr_filter_name,
                  ptr_iunknown,
                  &filter_lock_,
                  CLSID_AudioSinkFilter) {
  if (!ptr_frame_callback) {
    *ptr_result = E_INVALIDARG;
    return;
  }
  ptr_frame_callback_ = ptr_frame_callback;
  sink_pin_.reset(
      new (std::nothrow) AudioSinkPin(NAME("VideoSinkInputPin"),  // NOLINT
                                      this, &filter_lock_, ptr_result,
                                      L"VideoSink"));
  if (!sink_pin_) {
    *ptr_result = E_OUTOFMEMORY;
  } else {
    *ptr_result = S_OK;
  }
}

AudioSinkFilter::~AudioSinkFilter() {
}

// Locks filter and returns |AudioSinkPin::config|.
HRESULT AudioSinkFilter::config(VideoConfig* ptr_config) {
  CAutoLock lock(&filter_lock_);
  return sink_pin_->config(ptr_config);
}

// Locks filter and returns |AudioSinkPin::set_config|.
HRESULT AudioSinkFilter::set_config(const VideoConfig& config) {
  if (m_State != State_Stopped) {
    return VFW_E_NOT_STOPPED;
  }
  CAutoLock lock(&filter_lock_);
  return sink_pin_->set_config(config);
}

// Locks filter and returns AudioSinkPin pointer wrapped by |sink_pin_|.
CBasePin* AudioSinkFilter::GetPin(int index) {
  CBasePin* ptr_pin = NULL;
  CAutoLock lock(&filter_lock_);
  if (index == 0) {
    ptr_pin = sink_pin_.get();
  }
  return ptr_pin;
}

// Lock owned by |AudioSinkPin::Receive|. Copies buffer from |ptr_sample|
HRESULT AudioSinkFilter::OnSamplesReceived(IMediaSample* ptr_sample) {
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

  const int32 status =
      frame_.Init(sink_pin_->video_format_,
                  true,  // uncompressed frames are always "keyframes"
                  width,
                  height,
                  sink_pin_->stride_,
                  timestamp,
                  duration,
                  ptr_sample_buffer,
                  ptr_sample->GetActualDataLength());
  if (status) {
    LOG(ERROR) << "OnFrameReceived frame init failed: " << status;
    return E_FAIL;
  }
  LOG(INFO) << "OnFrameReceived received a frame:"
            << " width=" << width
            << " height=" << height
            << " stride=" << sink_pin_->stride_
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
