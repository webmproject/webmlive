// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "client_encoder/win/audio_sink_filter.h"

#include <dvdmedia.h>
#include <mmreg.h>
#include <vfwmsgs.h>

#include "client_encoder/win/dshow_util.h"
#include "client_encoder/win/media_source_dshow.h"
#include "client_encoder/win/media_type_dshow.h"
#include "client_encoder/win/webm_guids.h"
#include "glog/logging.h"

namespace webmlive {

///////////////////////////////////////////////////////////////////////////////
// AudioSinkPin
//

const GUID AudioSinkPin::kInputSubTypes[kNumInputSubTypes] = {
  MEDIASUBTYPE_IEEE_FLOAT,
  MEDIASUBTYPE_PCM
};

AudioSinkPin::AudioSinkPin(TCHAR* ptr_object_name,
                           AudioSinkFilter* ptr_filter,
                           CCritSec* ptr_filter_lock,
                           HRESULT* ptr_result,
                           LPCWSTR ptr_pin_name)
    : CBaseInputPin(ptr_object_name, ptr_filter, ptr_filter_lock, ptr_result,
                    ptr_pin_name) {
}

AudioSinkPin::~AudioSinkPin() {
}

// Returns preferred media types.
HRESULT AudioSinkPin::GetMediaType(int32 type_index,
                                   CMediaType* ptr_media_type) {
  if (type_index < 0 || !ptr_media_type) {
    return E_INVALIDARG;
  }
  if (type_index > kNumInputSubTypes) {
    return VFW_S_NO_MORE_ITEMS;
  }
  ptr_media_type->SetType(&MEDIATYPE_Audio);
  ptr_media_type->SetFormatType(&FORMAT_WaveFormatEx);
  ptr_media_type->SetSubtype(&kInputSubTypes[type_index]);
  return S_OK;
}

HRESULT AudioSinkPin::CheckMediaType(const CMediaType* ptr_media_type) {
  // Confirm media type is acceptable.
  const GUID* const ptr_type_guid = ptr_media_type->Type();
  if (!ptr_type_guid || *ptr_type_guid != MEDIATYPE_Audio) {
    LOG(INFO) << "rejecting type: majortype not audio.";
    return VFW_E_TYPE_NOT_ACCEPTED;
  }

  if (ptr_media_type->bTemporalCompression) {
    LOG(INFO) << "rejecting type: compressed audio.";
    return VFW_E_TYPE_NOT_ACCEPTED;
  }

  // Confirm that subtype and formattype GUIDs can be obtained.
  const GUID* const ptr_subtype_guid = ptr_media_type->Subtype();
  const GUID* const ptr_format_guid = ptr_media_type->FormatType();
  if (!ptr_subtype_guid || !ptr_format_guid) {
    LOG(INFO) << "invalid media type: missing subtype or formattype.";
    return E_INVALIDARG;
  }

  const GUID& format_guid = *ptr_format_guid;
  if (format_guid != FORMAT_WaveFormatEx) {
    LOG(INFO) << "rejecting type: format not FORMAT_WaveFormatEx.";
  }

  const GUID& subtype_guid = *ptr_subtype_guid;
  if (subtype_guid != MEDIASUBTYPE_PCM &&
      subtype_guid != MEDIASUBTYPE_IEEE_FLOAT) {
    LOG(INFO) << "rejecting type: subtype not PCM or IEEE_FLOAT.";
    return VFW_E_TYPE_NOT_ACCEPTED;
  }

  AudioMediaType format;
  int status = format.Init(*ptr_media_type);
  if (status) {
    LOG(INFO) << "invalid media type: AudioMediaType Init failed. " << status;
    return E_INVALIDARG;
  }

  if (format.format_tag() != WAVE_FORMAT_PCM &&
      format.format_tag() != WAVE_FORMAT_IEEE_FLOAT &&
      format.format_tag() != WAVE_FORMAT_EXTENSIBLE) {
    LOG(INFO) << "rejecting type: format tag not supported. "
              << format.format_tag();
    return VFW_E_TYPE_NOT_ACCEPTED;
  }

  const uint16 kPcm16Size = 16;
  const uint16 kIeeeFloatSize = 32;
  if (format.bits_per_sample() != kPcm16Size &&
      format.bits_per_sample() != kIeeeFloatSize) {
    LOG(INFO) << "rejecting type: sample size not supported. "
              << format.bits_per_sample();
    return VFW_E_TYPE_NOT_ACCEPTED;
  }

  actual_config_.format_tag = format.format_tag();
  actual_config_.channels = format.channels();
  actual_config_.sample_rate = format.sample_rate();
  actual_config_.bytes_per_second = format.bytes_per_second();
  actual_config_.block_align = format.block_align();
  actual_config_.bits_per_sample = format.bits_per_sample();
  actual_config_.valid_bits_per_sample = format.valid_bits_per_sample();
  actual_config_.channel_mask = format.channel_mask();

  LOG(INFO) << "\n CheckMediaType actual audio settings\n"
            << "   format_tag=" << actual_config_.format_tag << "\n"
            << "   channels=" << actual_config_.channels << "\n"
            << "   sample_rate=" << actual_config_.sample_rate << "\n"
            << "   bytes_per_second=" << actual_config_.bytes_per_second << "\n"
            << "   block_align=" << actual_config_.block_align << "\n"
            << "   bits_per_sample=" << actual_config_.bits_per_sample << "\n"
            << "   valid_bits_per_sample="
            << actual_config_.valid_bits_per_sample << "\n"
            << "   channel_mask=0x" << (std::hex)
            << actual_config_.channel_mask;
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
  hr = ptr_filter->OnSamplesReceived(ptr_sample);
  if (FAILED(hr)) {
    LOG(ERROR) << "OnSamplesReceived failed. " << HRLOG(hr);
  }
  return S_OK;
}

// Copies |actual_config_| to |ptr_config|. Note that the filter lock is always
// held by caller, |VideoSinkFilter::config|.
HRESULT AudioSinkPin::config(AudioConfig* ptr_config) {
  if (!ptr_config) {
    return E_POINTER;
  }
  *ptr_config = actual_config_;
  return S_OK;
}

// Sets |requested_config_| and resets |actual_config_|. Filter lock always
// held by caller, |VideoSinkFilter::set_config|.
HRESULT AudioSinkPin::set_config(const AudioConfig& config) {
  requested_config_ = config;
  actual_config_ = AudioConfig();
  return S_OK;
}

///////////////////////////////////////////////////////////////////////////////
// AudioSinkFilter
//
AudioSinkFilter::AudioSinkFilter(
    const TCHAR* ptr_filter_name,
    LPUNKNOWN ptr_iunknown,
    AudioSamplesCallbackInterface* ptr_samples_callback,
    HRESULT* ptr_result)
    : CBaseFilter(ptr_filter_name,
                  ptr_iunknown,
                  &filter_lock_,
                  CLSID_AudioSinkFilter) {
  if (!ptr_samples_callback) {
    *ptr_result = E_INVALIDARG;
    return;
  }
  ptr_samples_callback_ = ptr_samples_callback;
  sink_pin_.reset(
      new (std::nothrow) AudioSinkPin(NAME("AudioSinkInputPin"),  // NOLINT
                                      this, &filter_lock_, ptr_result,
                                      L"AudioSink"));
  if (!sink_pin_) {
    *ptr_result = E_OUTOFMEMORY;
  } else {
    *ptr_result = S_OK;
  }
}

AudioSinkFilter::~AudioSinkFilter() {
}

// Locks filter and returns |AudioSinkPin::config|.
HRESULT AudioSinkFilter::config(AudioConfig* ptr_config) {
  CAutoLock lock(&filter_lock_);
  return sink_pin_->config(ptr_config);
}

// Locks filter and returns |AudioSinkPin::set_config|.
HRESULT AudioSinkFilter::set_config(const AudioConfig& config) {
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
    LOG(ERROR) << "OnSamplesReceived called with empty sample.";
    hr = (hr == S_OK) ? E_FAIL : hr;
    return hr;
  }
  int64 timestamp = 0;
  int64 duration = 0;
  REFERENCE_TIME start_time = 0;
  REFERENCE_TIME end_time = 0;
  hr = ptr_sample->GetTime(&start_time, &end_time);
  if (FAILED(hr)) {
    LOG(WARNING) << "OnSamplesReceived cannot get media time(s).";
  } else {
    timestamp = media_time_to_milliseconds(start_time);
    if (hr != VFW_S_NO_STOP_TIME) {
      duration = media_time_to_milliseconds(end_time) - timestamp;
    } else {
      LOG(WARNING) << "OnSamplesReceived sample has no stop time.";
    }
  }
  return S_OK;
}

}  // namespace webmlive
