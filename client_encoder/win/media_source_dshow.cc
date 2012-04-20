// Copyright (c) 2012 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "client_encoder/win/media_source_dshow.h"

#include <initguid.h>  // MUST be included before VorbisTypes.h to avoid
                       // undefined external error for
                       // IID_VorbisEncodeSettings due to behavior of
                       // DEFINE_GUID macro.
#include <vfwmsgs.h>

#include <sstream>

#include "boost/scoped_array.hpp"
#include "client_encoder/video_encoder.h"
#include "client_encoder/webm_encoder.h"
#include "client_encoder/win/dshow_util.h"
#include "client_encoder/win/media_type_dshow.h"
#include "client_encoder/win/video_sink_filter.h"
#include "client_encoder/win/webm_guids.h"
#include "glog/logging.h"
#include "oggdsf/IVorbisEncodeSettings.h"
#include "oggdsf/VorbisTypes.h"
#include "webmdshow/IDL/vp8encoderidl.h"
#include "webmdshow/IDL/webmmuxidl.h"

namespace webmlive {

namespace {
// DirectShow Filter name constants.
const wchar_t* const kVideoSourceName = L"VideoSource";
const wchar_t* const kVideoSinkName = L"VideoSink";
const wchar_t* const kAudioSourceName = L"AudioSource";

// Converts a std::string to std::wstring.
std::wstring string_to_wstring(const std::string& str) {
  std::wostringstream wstr;
  wstr << str.c_str();
  return wstr.str();
}

// Converts |wstr| to a multi-byte string and returns result std::string.
std::string wstring_to_string(const std::wstring& wstr) {
  // Conversion buffer for |wcstombs| calls.
  const size_t buf_size = wstr.length() + 1;
  boost::scoped_array<char> temp_str(
      new (std::nothrow) char[buf_size]);   // NOLINT
  if (!temp_str) {
    LOG(ERROR) << "can't convert wstring of length=" << wstr.length();
    return std::string("<empty>");
  }
  memset(temp_str.get(), 0, buf_size);
  size_t num_converted = 0;
  wcstombs_s(&num_converted, temp_str.get(), buf_size, wstr.c_str(),
             wstr.length()*sizeof(wchar_t));
  std::string str = temp_str.get();
  return str;
}
}  // anonymous namespace

// Converts media time (100 nanosecond ticks) to milliseconds.
int64 media_time_to_milliseconds(REFERENCE_TIME media_time) {
  return media_time / 10000;
}

// Converts media time (100 nanosecond ticks) to seconds.
double media_time_to_seconds(REFERENCE_TIME media_time) {
  return media_time / 10000000.0;
}

// Converts seconds to media time (100 nanosecond ticks).
REFERENCE_TIME seconds_to_media_time(double seconds) {
  return static_cast<REFERENCE_TIME>(seconds * 10000000);
}

MediaSourceImpl::MediaSourceImpl()
    : audio_from_video_source_(false),
      media_event_handle_(INVALID_HANDLE_VALUE),
      ptr_video_callback_(NULL) {
}

MediaSourceImpl::~MediaSourceImpl() {
  // Manually release directshow interfaces to avoid problems related to
  // destruction order of com_ptr_t members.
  audio_source_ = 0;
  video_source_ = 0;
  video_sink_ = 0;
  media_event_handle_ = INVALID_HANDLE_VALUE;
  media_control_ = 0;
  media_event_ = 0;
  capture_graph_builder_ = 0;
  graph_builder_ = 0;
  CoUninitialize();
}

// Builds a DirectShow filter graph that looks like this:
// video source -> video sink
int MediaSourceImpl::Init(const WebmEncoderConfig& config,
                          VideoFrameCallbackInterface* ptr_video_callback) {
  if (!ptr_video_callback) {
    LOG(ERROR) << "Null VideoFrameCallbackInterface.";
    return kInvalidArg;
  }
  ptr_video_callback_ = ptr_video_callback;
  requested_audio_config_ = config.requested_audio_config;
  requested_video_config_ = config.requested_video_config;
  ui_opts_ = config.ui_opts;
  const HRESULT hr = CoInitialize(NULL);
  if (FAILED(hr)) {
    LOG(ERROR) << "CoInitialize failed: " << HRLOG(hr);
    return WebmEncoder::kInitFailed;
  }
  int status = CreateGraph();
  if (status) {
    LOG(ERROR) << "CreateGraphInterfaces failed: " << status;
    return WebmEncoder::kInitFailed;
  }
  if (!config.video_device_name.empty()) {
    video_device_name_ = string_to_wstring(config.video_device_name);
  }
  status = CreateVideoSource();
  if (status) {
    LOG(ERROR) << "CreateVideoSource failed: " << status;
    return WebmEncoder::kNoVideoSource;
  }
  status = CreateVideoSink();
  if (status) {
    LOG(ERROR) << "CreateVideoSink failed: " << status;
    return WebmEncoder::kNoVideoSource;
  }
  status = ConnectVideoSourceToVideoSink();
  if (status) {
    LOG(ERROR) << "ConnectVideoSourceToVideoSink failed: " << status;
    return WebmEncoder::kVideoSinkError;
  }
  status = InitGraphControl();
  if (status) {
    LOG(ERROR) << "ConnectVideoSourceToVideoSink failed: " << status;
    return WebmEncoder::kEncodeMonitorError;
  }
  return kSuccess;
}

// Runs the filter graph via |IMediaControl::Run|. Note that the Run call is
// asynchronous, and typically returns S_FALSE to report that the run request
// is in progress but has not completed.
int MediaSourceImpl::Run() {
  CoInitialize(NULL);
  HRESULT hr = media_control_->Run();
  if (FAILED(hr)) {
    LOG(ERROR) << "media control Run failed, cannot run capture!" << HRLOG(hr);
    return WebmEncoder::kRunFailed;
  }
  return kSuccess;
}

// Confirms that the filter graph is running via use of |HandleMediaEvent| to
// check for abort and completion events. Actual FILTER_STATE (as reported by
// |IMediaControl::GetState|) is ignored to avoid complicating |CheckStatus|
// with code that waits for the transition from |State_Stopped| to
// |State_Running|.
int MediaSourceImpl::CheckStatus() {
  int status = HandleMediaEvent();
  if (status == kGraphAborted || status == kGraphCompleted) {
    LOG(ERROR) << "Capture graph stopped!";
    return WebmEncoder::kAVCaptureStopped;
  }
  return kSuccess;
}

// Stops the filter graph via call to |IMediaControl::Stop|.
void MediaSourceImpl::Stop() {
  const HRESULT hr = media_control_->Stop();
  if (FAILED(hr)) {
    LOG(ERROR) << "media control Stop failed! error=" << HRLOG(hr);
  } else {
    LOG(INFO) << "graph stopping. status=" << HRLOG(hr);
  }
  CoUninitialize();
}

// Creates the graph builder, |graph_builder_|, and capture graph builder,
// |capture_graph_builder_|, and passes |graph_builder_| to
// |capture_graph_builder_|.
int MediaSourceImpl::CreateGraph() {
  HRESULT hr = graph_builder_.CreateInstance(CLSID_FilterGraph);
  if (FAILED(hr)) {
    LOG(ERROR) << "graph builder creation failed." << HRLOG(hr);
    return kCannotCreateGraph;
  }
  hr = capture_graph_builder_.CreateInstance(CLSID_CaptureGraphBuilder2);
  if (FAILED(hr)) {
    LOG(ERROR) << "capture graph builder creation failed." << HRLOG(hr);
    return kCannotCreateGraph;
  }
  hr = capture_graph_builder_->SetFiltergraph(graph_builder_);
  if (FAILED(hr)) {
    LOG(ERROR) << "could not set capture builder graph." << HRLOG(hr);
    return kGraphConfigureError;
  }
  return kSuccess;
}

// Uses |CaptureSourceLoader| to find a video capture source.  If successful
// an instance of the source filter is created and added to the filter graph.
// Note: the first device found is used unconditionally.
int MediaSourceImpl::CreateVideoSource() {
  CaptureSourceLoader loader;
  int status = loader.Init(CLSID_VideoInputDeviceCategory);
  if (status) {
    LOG(ERROR) << "no video source!";
    return WebmEncoder::kNoVideoSource;
  }
  for (int i = 0; i < loader.GetNumSources(); ++i) {
    LOG(INFO) << "vdev" << i << ": "
              << wstring_to_string(loader.GetSourceName(i).c_str());
  }
  if (video_device_name_.empty()) {
    video_source_ = loader.GetSource(0);
  } else {
    video_source_ = loader.GetSource(video_device_name_);
  }
  if (!video_source_) {
    LOG(ERROR) << "cannot create video source!";
    return WebmEncoder::kNoVideoSource;
  }
  const HRESULT hr = graph_builder_->AddFilter(video_source_,
                                               kVideoSourceName);
  if (FAILED(hr)) {
    LOG(ERROR) << "cannot add video source to graph." << HRLOG(hr);
    return kCannotAddFilter;
  }
  // TODO(tomfinegan): set video format instead of hoping for sane defaults.
  return kSuccess;
}

int MediaSourceImpl::CreateVideoSink() {
  HRESULT status = E_FAIL;
  const std::string filter_name = wstring_to_string(kVideoSinkName);
  VideoSinkFilter* const ptr_filter =
      new (std::nothrow) VideoSinkFilter(filter_name.c_str(),  // NOLINT
                                         NULL,
                                         ptr_video_callback_,
                                         &status);
  if (!ptr_filter || FAILED(status)) {
    delete ptr_filter;
    LOG(ERROR) << "VideoSinkFilter construction failed" << HRLOG(status);
    return kVideoSinkCreateError;
  }
  video_sink_ = ptr_filter;
  status = graph_builder_->AddFilter(video_sink_, kVideoSinkName);
  if (FAILED(status)) {
    LOG(ERROR) << "cannot add video sink to graph" << HRLOG(status);
    return kCannotAddFilter;
  }
  return kSuccess;
}

int MediaSourceImpl::ConnectVideoSourceToVideoSink() {
  PinFinder pin_finder;
  int status = pin_finder.Init(video_source_);
  if (status) {
    LOG(ERROR) << "cannot look for pins on video source!";
    return kVideoConnectError;
  }
  IPinPtr video_source_pin = pin_finder.FindVideoOutputPin(0);
  if (!video_source_pin) {
    LOG(ERROR) << "cannot find output pin on video source!";
    return kVideoConnectError;
  }
  status = pin_finder.Init(video_sink_);
  if (status) {
    LOG(ERROR) << "cannot look for pins on video sink!";
    return kVideoConnectError;
  }
  IPinPtr sink_input_pin = pin_finder.FindVideoInputPin(0);
  if (!sink_input_pin) {
    LOG(ERROR) << "cannot find video input pin on video sink filter!";
    return kVideoConnectError;
  }
  status = kVideoConnectError;
  HRESULT hr = E_FAIL;
  for (int i = 0; i < kVideoFormatCount && hr != S_OK; ++i) {
    MediaTypePtr accepted_type;
    status = ConfigureVideoSource(video_source_pin, i, &accepted_type);
    if (status == kSuccess) {
      LOG(INFO) << "Format " << i << " configuration OK.";
    } else {
      continue;
    }
    hr = graph_builder_->ConnectDirect(video_source_pin, sink_input_pin,
                                       accepted_type.get());
    LOG(INFO) << "Format " << i << ((hr == S_OK) ? " connected." : " failed.");
  }
  if (status || hr != S_OK) {
    // All previous connection attempts failed. Try one last time using
    // |video_source_pin|'s default format.
    PinFormat formatter(video_source_pin);
    status = formatter.set_format(NULL);
    if (status == kSuccess) {
      hr = graph_builder_->ConnectDirect(video_source_pin, sink_input_pin,
                                         NULL);
      if (hr == S_OK) {
        LOG(INFO) << "Connected with video source pin default format.";
      } else {
        LOG(ERROR) << "Cannot connect video source to VP8 encoder.";
        status = kVideoConnectError;
      }
    }
  }
  if (status == kSuccess && hr == S_OK) {
    AM_MEDIA_TYPE media_type = {0};
    // Store the actual width/height/frame rate.
    hr = video_source_pin->ConnectionMediaType(&media_type);
    if (hr == S_OK) {
      VideoMediaType video_format;
      if (video_format.Init(media_type) == kSuccess) {
        LOG(INFO) << "actual capture width=" << video_format.width()
                  << " height=" << video_format.height()
                  << " frame_rate=" << video_format.frame_rate();
        actual_video_config_.width = video_format.width();
        actual_video_config_.height = video_format.height();
        actual_video_config_.frame_rate = video_format.frame_rate();
      }
    }
    MediaType::FreeMediaTypeData(&media_type);
  }
  return status;
}

// Attempts to configure |video_source_pin| media type through use of user
// settings stored in |config_.requested_video_config| with |VideoMediaType|
// to produce an AM_MEDIA_TYPE struct suitable for use with
// IAMStreamConfig::SetFormat. Returns |kSuccess| upon successful
// configuration.
int MediaSourceImpl::ConfigureVideoSource(const IPinPtr& pin,
                                          int sub_type,
                                          MediaTypePtr* ptr_type) {
  if (!ptr_type) {
    LOG(ERROR) << "NULL media type output pointer!";
    return kInvalidArg;
  }
  if (ui_opts_.manual_video_config) {
    // Always disable manual configuration.
    // |ConfigureVideoSource| is called in a loop, so this avoids making the
    // user mess with the dialog repeatedly if/when manual settings disagree
    // with the video sink filter.
    ui_opts_.manual_video_config = false;

    // Try showing |video_source_|'s property page.
    bool filter_config_ok = false;
    HRESULT hr = ShowFilterPropertyPage(video_source_);
    if (FAILED(hr)) {
      LOG(WARNING) << "Unable to show video source filter property page."
                   << HRLOG(hr);
    } else {
      filter_config_ok = true;
    }

    // Try showing the pin property page. Extremely common video sources (I.E.
    // Logitech webcams) show vastly different configuration options on the
    // filter and pin property pages.
    bool pin_config_ok = false;
    hr = ShowPinPropertyPage(pin);
    if (FAILED(hr)) {
      LOG(WARNING) << "Unable to show video source pin property page."
                   << HRLOG(hr);
    } else {
      pin_config_ok = true;
    }
    if (filter_config_ok || pin_config_ok) {
      PinFormat formatter(pin);
      const int status = ptr_type->Attach(formatter.format());
      if (status) {
        LOG(WARNING) << "Manual video config failed: user config successful, "
                        "but pin has no media type.";

        // Fall through and try configuring the pin without user intervention.
      } else {
        LOG(INFO) << "Manual video configuration successful.";
        return kSuccess;
      }
    }

    // Fall through and use settings in |video_config| when property page
    // stuff fails.
    LOG(WARNING) << "Manual video configuration failed.";
  }
  VideoMediaType video_format;
  int status = video_format.Init(MEDIATYPE_Video, FORMAT_VideoInfo);
  if (status) {
    LOG(ERROR) << "video media type init failed.";
    return kVideoConfigureError;
  }
  const VideoFormat video_sub_type = static_cast<VideoFormat>(sub_type);
  const VideoConfig& video_config = requested_video_config_;
  if (video_config.width != 0 ||
      video_config.height != 0 ||
      video_config.frame_rate != 0) {
    // User specified video settings are present: build a complete media type
    // and attempt configuration of the source device.
    status = video_format.ConfigureSubType(video_sub_type, video_config);
    if (status) {
      LOG(ERROR) << "video sub type configuration failed.";
      return kVideoConfigureError;
    }
    PinFormat formatter(pin);
    status = formatter.set_format(
        const_cast<AM_MEDIA_TYPE*>(video_format.get()));
    if (status) {
      LOG(WARNING) << "cannot set pin format: " << status;
      return kVideoConfigureError;
    }
    status = ptr_type->Attach(formatter.format());
    if (status) {
      LOG(ERROR) << "Cannot attach to pin format: " << status;
      return kVideoConfigureError;
    }
  } else {
    // No user settings: configure a partial media type.
    status = video_format.ConfigurePartialType(video_sub_type);
    if (status) {
      LOG(ERROR) << "cannot set partial pin format: " << status;
      return kVideoConfigureError;
    }
    status = ptr_type->Copy(video_format.get());
    if (status) {
      LOG(ERROR) << "Cannot copy partial type: " << status;
      return kVideoConfigureError;
    }
  }
  return WebmEncoder::kSuccess;
}

int MediaSourceImpl::InitGraphControl() {
  media_control_ = IMediaControlPtr(graph_builder_);
  if (!media_control_) {
    LOG(ERROR) << "cannot create media control.";
    return WebmEncoder::kEncodeControlError;
  }
  media_event_ = IMediaEventPtr(graph_builder_);
  if (!media_event_) {
    LOG(ERROR) << "cannot create media event.";
    return WebmEncoder::kEncodeMonitorError;
  }
  OAEVENT* const ptr_handle = reinterpret_cast<OAEVENT*>(&media_event_handle_);
  const HRESULT hr = media_event_->GetEventHandle(ptr_handle);
  if (FAILED(hr)) {
    LOG(ERROR) << "could not media event handle!" << HRLOG(hr);
    return WebmEncoder::kEncodeMonitorError;
  }
  return kSuccess;
}

// Checks for an audio output pin on |video_source_|.  If one exists
// |video_source_| is copied to |audio_source_| and |kSuccess| is returned.
// If there is no audio output pin |CaptureSourceLoader| is used to find an
// audio capture source.  If successful an instance of the source filter is
// created and added to the filter graph.
// Note: in the |CaptureSourceLoader| case, the first device found is used
// unconditionally.
int MediaSourceImpl::CreateAudioSource() {
  // Check for an audio pin on the video source.
  // TODO(tomfinegan): We assume that the user wants to use the audio feed
  //                   exposed by the video capture source. This behavior
  //                   should be configurable.
  PinFinder pin_finder;
  int status = pin_finder.Init(video_source_);
  if (status) {
    LOG(ERROR) << "cannot check video source for audio pins!";
    return WebmEncoder::kInitFailed;
  }
  if (pin_finder.FindAudioOutputPin(0)) {
    // Use the video source filter audio output pin.
    LOG(INFO) << "Using video source filter audio output pin.";
    audio_source_ = video_source_;
    audio_from_video_source_ = true;
  } else {
    // The video source doesn't have an audio output pin. Find an audio
    // capture source.
    CaptureSourceLoader loader;
    status = loader.Init(CLSID_AudioInputDeviceCategory);
    if (status) {
      LOG(ERROR) << "no audio source!";
      return WebmEncoder::kNoAudioSource;
    }
    for (int i = 0; i < loader.GetNumSources(); ++i) {
      LOG(INFO) << "adev" << i << ": "
                << wstring_to_string(loader.GetSourceName(i).c_str());
    }
    if (audio_device_name_.empty()) {
      audio_source_ = loader.GetSource(0);
    } else {
      audio_source_ = loader.GetSource(audio_device_name_);
    }
    if (!audio_source_) {
      LOG(ERROR) << "cannot create audio source!";
      return WebmEncoder::kNoAudioSource;
    }
    const HRESULT hr = graph_builder_->AddFilter(audio_source_,
                                                 kAudioSourceName);
    if (FAILED(hr)) {
      LOG(ERROR) << "cannot add audio source to graph." << HRLOG(hr);
      return kCannotAddFilter;
    }
  }
  return kSuccess;
}

// Configures audio source pin to match user settings.  Attempts to find a
// matching media type, and uses it if successful. Constructs |AudioMediaType|
// to configure pin with an AM_MEDIA_TYPE matching the user's settings if no
// match is found. Returns kSuccess upon successful configuration.
int MediaSourceImpl::ConfigureAudioSource(const IPinPtr& pin) {
  if (ui_opts_.manual_audio_config) {
    bool filter_config_ok = false, pin_config_ok = false;
    // Manual audio configuration is enabled; try showing |audio_source_|'s
    // property page, but only if the audio source is not a pin on
    // |video_source_|. This is done to avoid any potential problems within
    // the source filter that could be created by making filter level
    // configuration changes through the property page after the video pin
    // has been connected to the VP8 encoder filter.
    // Unlike with video, only one of the filter and pin property pages are
    // shown. This is because they're often the same dialog for audio sources.
    if (!audio_from_video_source_) {
      HRESULT hr = ShowFilterPropertyPage(audio_source_);
      if (FAILED(hr)) {
        LOG(WARNING) << "Unable to show audio source filter property page."
                     << HRLOG(hr);
      } else {
        filter_config_ok = true;
      }
    } else {
      HRESULT hr = ShowPinPropertyPage(pin);
      if (FAILED(hr)) {
        LOG(WARNING) << "Unable to show video source pin property page."
                     << HRLOG(hr);
      } else {
        pin_config_ok = true;
      }
    }
    if (filter_config_ok || pin_config_ok) {
      LOG(INFO) << "Manual audio configuration successful.";
      return kSuccess;
    }
    // Fall through and use settings in |config_.audio_config| when property
    // page stuff fails.
    LOG(WARNING) << "Manual audio configuration failed.";
  }
  PinFormat formatter(pin);
  MediaTypePtr audio_format;
  int status = audio_format.Attach(
      formatter.FindMatchingFormat(requested_audio_config_));
  if (status) {
    // Try directly configuring the pin with user settings.
    LOG(WARNING) << "no format matching requested audio settings.";
    AudioMediaType user_audio_format;
    status = user_audio_format.Init();
    if (status) {
      LOG(ERROR) << "audio media type init failed, status=" << status;
      return WebmEncoder::kAudioConfigureError;
    }
    status = user_audio_format.Configure(requested_audio_config_);
    if (status) {
      LOG(ERROR) << "audio media type configuration failed, status=" << status;
      return WebmEncoder::kAudioConfigureError;
    }
    status = formatter.set_format(user_audio_format.get());
    if (status) {
      LOG(ERROR) << "pin did not accept user audio format, status=" << status;
      return WebmEncoder::kAudioConfigureError;
    }
  } else {
    // Pin lists a format matching user settings; use it.
    status = formatter.set_format(audio_format.get());
    if (status) {
      LOG(ERROR) << "pin did not accept user audio format, status=" << status;
      return WebmEncoder::kAudioConfigureError;
    }
  }
  return kSuccess;
}

  }
    return kCannotAddFilter;
  }
  return kSuccess;
}

// Checks |media_event_handle_| and reads the event from |media_event_| when
// signaled.  Responds only to completion and error events.
int MediaSourceImpl::HandleMediaEvent() {
  int status = kSuccess;
  const DWORD wait_status = WaitForSingleObject(media_event_handle_, 0);
  const bool media_event_recvd = (wait_status == WAIT_OBJECT_0);
  if (media_event_recvd) {
    long code = 0;    // NOLINT
    long param1 = 0;  // NOLINT
    long param2 = 0;  // NOLINT
    const HRESULT hr = media_event_->GetEvent(&code, &param1, &param2, 0);
    media_event_->FreeEventParams(code, param1, param2);
    if (SUCCEEDED(hr)) {
      if (code == EC_ERRORABORT) {
        LOG(ERROR) << "EC_ERRORABORT";
        status = kGraphAborted;
      } else if (code == EC_ERRORABORTEX) {
        LOG(ERROR) << "EC_ERRORABORTEX";
        status = kGraphAborted;
      } else if (code == EC_USERABORT) {
        LOG(ERROR) << "EC_USERABORT";
        status = kGraphAborted;
      } else if (code == EC_COMPLETE) {
        LOG(INFO) << "EC_COMPLETE";
        status = kGraphCompleted;
      }
    } else {
      // Couldn't get the event; tell caller to abort.
      LOG(ERROR) << "GetEvent failed: " << HRLOG(hr);
      status = kGraphAborted;
    }
  }
  return status;
}

///////////////////////////////////////////////////////////////////////////////
// CaptureSourceLoader
//

CaptureSourceLoader::CaptureSourceLoader()
    : source_type_(GUID_NULL) {
}

CaptureSourceLoader::~CaptureSourceLoader() {
}

// Verifies that |source_type| is known and calls |FindAllSources|.
int CaptureSourceLoader::Init(CLSID source_type) {
  if (source_type != CLSID_AudioInputDeviceCategory &&
      source_type != CLSID_VideoInputDeviceCategory) {
    LOG(ERROR) << "unknown device category!";
    return WebmEncoder::kInvalidArg;
  }
  source_type_ = source_type;
  return FindAllSources();
}

// Enumerates input devices of type |source_type_| and adds them to the map of
// sources, |sources_|.
int CaptureSourceLoader::FindAllSources() {
  ICreateDevEnumPtr sys_enum;
  HRESULT hr = sys_enum.CreateInstance(CLSID_SystemDeviceEnum);
  if (FAILED(hr)) {
    LOG(ERROR) << "source enumerator creation failed." << HRLOG(hr);
    return kNoDeviceFound;
  }
  const DWORD kNoEnumFlags = 0;
  hr = sys_enum->CreateClassEnumerator(source_type_, &source_enum_,
                                       kNoEnumFlags);
  if (FAILED(hr) || hr == S_FALSE) {
    LOG(ERROR) << "moniker creation failed (no devices)." << HRLOG(hr);
    return kNoDeviceFound;
  }
  int source_index = 0;
  for (;;) {
    IMonikerPtr source_moniker;
    hr = source_enum_->Next(1, &source_moniker, NULL);
    if (FAILED(hr) || hr == S_FALSE || !source_moniker) {
      LOG(INFO) << "Done enumerating sources, found " << source_index
                << " sources.";
      break;
    }
    std::wstring name = GetMonikerFriendlyName(source_moniker);
    if (name.empty()) {
      LOG(WARNING) << "source=" << source_index << " has no name, skipping.";
      continue;
    }
    LOG(INFO) << "source=" << source_index << " name="
              << wstring_to_string(name.c_str());
    sources_[source_index] = name;
    ++source_index;
  }
  if (sources_.size() == 0) {
    LOG(ERROR) << "No devices found!";
    return kNoDeviceFound;
  }
  return kSuccess;
}

// Obtains source name for |index| and returns result of
// |GetSource(std::wstring)|.
IBaseFilterPtr CaptureSourceLoader::GetSource(int index) {
  if (static_cast<size_t>(index) >= sources_.size()) {
    LOG(ERROR) << "" << index << " is not a valid source index";
    return NULL;
  }
  return GetSource(GetSourceName(index));
}

// Resets |source_enum_| and enumerates video input sources until one matching
// |name| is found. Then creates an instance of the filter by calling
// |BindToObject| on the device moniker (|source_moniker|) returned by the
// enumerator.
IBaseFilterPtr CaptureSourceLoader::GetSource(const std::wstring name) {
  if (name.empty()) {
    LOG(ERROR) << "empty source name.";
    return NULL;
  }
  HRESULT hr = source_enum_->Reset();
  if (FAILED(hr)) {
    LOG(ERROR) << "cannot reset source enumerator!" << HRLOG(hr);
    return NULL;
  }
  IMonikerPtr source_moniker;
  for (;;) {
    hr = source_enum_->Next(1, &source_moniker, NULL);
    if (FAILED(hr) || hr == S_FALSE || !source_moniker) {
      LOG(ERROR) << "device not found!";
      return NULL;
    }
    std::wstring source_name = GetMonikerFriendlyName(source_moniker);
    if (source_name == name) {
      break;
    }
  }
  IBaseFilterPtr filter = NULL;
  hr = source_moniker->BindToObject(NULL, NULL, IID_IBaseFilter,
                                    reinterpret_cast<void**>(&filter));
  if (FAILED(hr)) {
    LOG(ERROR) << "cannot bind filter!" << HRLOG(hr);
  }
  return filter;
}

// Extracts a string value from a |VARIANT|.  Returns emptry string on failure.
std::wstring CaptureSourceLoader::GetStringProperty(
    const IPropertyBagPtr &prop_bag,
    std::wstring prop_name) {
  VARIANT var;
  VariantInit(&var);
  const HRESULT hr = prop_bag->Read(prop_name.c_str(), &var, NULL);
  std::wstring name;
  if (SUCCEEDED(hr)) {
    name = V_BSTR(&var);
  }
  VariantClear(&var);
  return name;
}

// Returns the value of |moniker|'s friendly name property.  Returns an empty
// std::wstring on failure.
std::wstring CaptureSourceLoader::GetMonikerFriendlyName(
    const IMonikerPtr& moniker) {
  std::wstring name;
  if (moniker) {
    IPropertyBagPtr props;
    HRESULT hr = moniker->BindToStorage(0, 0, IID_IPropertyBag,
                                        reinterpret_cast<void**>(&props));
    if (hr == S_OK) {
      const wchar_t* const kFriendlyName = L"FriendlyName";
      name = GetStringProperty(props, kFriendlyName);
      if (name.empty()) {
        LOG(WARNING) << "moniker friendly name property missing or empty.";
      }
    }
  }
  return name;
}

///////////////////////////////////////////////////////////////////////////////
// PinFinder
//

PinFinder::PinFinder() {
}

PinFinder::~PinFinder() {
}

// Creates the pin enumerator, |pin_enum_|
int PinFinder::Init(const IBaseFilterPtr& filter) {
  if (!filter) {
    LOG(ERROR) << "NULL filter.";
    return WebmEncoder::kInvalidArg;
  }
  const HRESULT hr = filter->EnumPins(&pin_enum_);
  if (FAILED(hr)) {
    LOG(ERROR) << "cannot enum filter pins!" << HRLOG(hr);
    return WebmEncoder::kInitFailed;
  }
  return WebmEncoder::kSuccess;
}

// Enumerates pins and uses |PinInfo| to find audio input pins while
// incrementing |num_found| until |index| is reached. Returns empty |IPinPtr|
// on failure.
IPinPtr PinFinder::FindAudioInputPin(int index) const {
  IPinPtr pin;
  int num_found = 0;
  for (;;) {
    const HRESULT hr = pin_enum_->Next(1, &pin, NULL);
    if (hr != S_OK) {
      break;
    }
    PinInfo pin_info(pin);
    if (pin_info.IsInput() && pin_info.IsAudio()) {
      ++num_found;
      if (num_found == index+1) {
        break;
      }
    }
  }
  return pin;
}

// Enumerates pins and uses |PinInfo| to find audio output pins while
// incrementing |num_found| until |index| is reached. Returns empty |IPinPtr|
// on failure.
IPinPtr PinFinder::FindAudioOutputPin(int index) const {
  IPinPtr pin;
  int num_found = 0;
  for (;;) {
    const HRESULT hr = pin_enum_->Next(1, &pin, NULL);
    if (hr != S_OK) {
      break;
    }
    PinInfo pin_info(pin);
    if (pin_info.IsOutput() && pin_info.IsAudio()) {
      ++num_found;
      if (num_found == index+1) {
        break;
      }
    }
  }
  return pin;
}

// Enumerates pins and uses |PinInfo| to find video input pins while
// incrementing |num_found| until |index| is reached. Returns empty |IPinPtr|
// on failure.
IPinPtr PinFinder::FindVideoInputPin(int index) const {
  IPinPtr pin;
  int num_found = 0;
  for (;;) {
    const HRESULT hr = pin_enum_->Next(1, &pin, NULL);
    if (hr != S_OK) {
      break;
    }
    PinInfo pin_info(pin);
    if (pin_info.IsInput() && pin_info.IsVideo()) {
      ++num_found;
      if (num_found == index+1) {
        break;
      }
    }
  }
  return pin;
}

// Enumerates pins and uses |PinInfo| to find video output pins while
// incrementing |num_found| until |index| is reached. Returns empty |IPinPtr|
// on failure.
IPinPtr PinFinder::FindVideoOutputPin(int index) const {
  IPinPtr pin;
  int num_found = 0;
  for (;;) {
    const HRESULT hr = pin_enum_->Next(1, &pin, NULL);
    if (hr != S_OK) {
      break;
    }
    PinInfo pin_info(pin);
    if (pin_info.IsOutput() && pin_info.IsVideo()) {
      ++num_found;
      if (num_found == index+1) {
        break;
      }
    }
  }
  return pin;
}

// Enumerates pins and uses |PinInfo| to find stream input pins while
// incrementing |num_found| until |index| is reached. Returns empty |IPinPtr|
// on failure.
IPinPtr PinFinder::FindStreamInputPin(int index) const {
  IPinPtr pin;
  int num_found = 0;
  for (;;) {
    const HRESULT hr = pin_enum_->Next(1, &pin, NULL);
    if (hr != S_OK) {
      break;
    }
    PinInfo pin_info(pin);
    if (pin_info.IsInput() && pin_info.IsStream()) {
      ++num_found;
      if (num_found == index+1) {
        break;
      }
    }
  }
  return pin;
}

// Enumerates pins and uses |PinInfo| to find stream output pins while
// incrementing |num_found| until |index| is reached. Returns empty |IPinPtr|
// on failure.
IPinPtr PinFinder::FindStreamOutputPin(int index) const {
  IPinPtr pin;
  int num_found = 0;
  for (;;) {
    const HRESULT hr = pin_enum_->Next(1, &pin, NULL);
    if (hr != S_OK) {
      break;
    }
    PinInfo pin_info(pin);
    if (pin_info.IsOutput() && pin_info.IsStream()) {
      ++num_found;
      if (num_found == index+1) {
        break;
      }
    }
  }
  return pin;
}

// Enumerates pins and uses |PinInfo| to find input pins while incrementing
// |num_found| until |index| is reached. Returns empty |IPinPtr| on failure.
IPinPtr PinFinder::FindInputPin(int index) const {
  IPinPtr pin;
  int num_found = 0;
  for (;;) {
    const HRESULT hr = pin_enum_->Next(1, &pin, NULL);
    if (hr != S_OK) {
      break;
    }
    PinInfo pin_info(pin);
    if (pin_info.IsInput()) {
      ++num_found;
      if (num_found == index+1) {
        break;
      }
    }
  }
  return pin;
}

///////////////////////////////////////////////////////////////////////////////
// PinInfo
//

// Construct |PinInfo| for |pin|.
PinInfo::PinInfo(const IPinPtr& pin)
    : pin_(pin) {
}

PinInfo::~PinInfo() {
}

// Enumerate media types on |pin_| until match for |major_type| is found or
// types are exhausted.
bool PinInfo::HasMajorType(GUID major_type) const {
  bool has_type = false;
  if (pin_) {
    IEnumMediaTypesPtr mediatype_enum;
    HRESULT hr = pin_->EnumMediaTypes(&mediatype_enum);
    if (SUCCEEDED(hr)) {
      for (;;) {
        AM_MEDIA_TYPE* ptr_media_type = NULL;
        hr = mediatype_enum->Next(1, &ptr_media_type, 0);
        if (hr != S_OK) {
          break;
        }
        has_type = (ptr_media_type && ptr_media_type->majortype == major_type);
        MediaType::FreeMediaTypeData(ptr_media_type);
        if (has_type) {
          break;
        }
      }
    }
  }
  return has_type;
}

// Returns true if |HasMajorType| returns true for |MEDIATYPE_Audio|.
bool PinInfo::IsAudio() const {
  bool is_audio_pin = false;
  if (pin_) {
    is_audio_pin = HasMajorType(MEDIATYPE_Audio);
  }
  return is_audio_pin;
}

// Returns true if |pin_->QueryDirection| succeeds and sets |direction| to
// |PINDIR_INPUT|.
bool PinInfo::IsInput() const {
  bool is_input = false;
  if (pin_) {
    PIN_DIRECTION direction;
    const HRESULT hr = pin_->QueryDirection(&direction);
    is_input = (hr == S_OK && direction == PINDIR_INPUT);
  }
  return is_input;
}

// Returns true if |pin_->QueryDirection| succeeds and sets |direction| to
// |PINDIR_OUTPUT|.
bool PinInfo::IsOutput() const {
  bool is_output = false;
  if (pin_) {
    PIN_DIRECTION direction;
    const HRESULT hr = pin_->QueryDirection(&direction);
    is_output = (hr == S_OK && direction == PINDIR_OUTPUT);
  }
  return is_output;
}

// Returns true if |HasMajorType| returns true for |MEDIATYPE_Video|.
bool PinInfo::IsVideo() const {
  bool is_video_pin = false;
  if (pin_) {
    is_video_pin = HasMajorType(MEDIATYPE_Video);
  }
  return is_video_pin;
}

// Returns true if |HasMajorType| returns true for |MEDIATYPE_Stream|.
bool PinInfo::IsStream() const {
  bool is_stream_pin = false;
  if (pin_) {
    is_stream_pin = HasMajorType(MEDIATYPE_Stream);
  }
  return is_stream_pin;
}

///////////////////////////////////////////////////////////////////////////////
// VideoPinInfo
//

VideoPinInfo::VideoPinInfo() {
}

VideoPinInfo::~VideoPinInfo() {
}

// Copies |pin| to |pin_| (results in an AddRef), and:
// - confirms that |pin| is a video pin using |PinInfo::IsVideo|, and
// - confirms that |pin| is connected.
int VideoPinInfo::Init(const IPinPtr& pin) {
  if (!pin) {
    LOG(ERROR) << "empty pin.";
    return kInvalidArg;
  }
  PinInfo info(pin);
  if (info.IsVideo() == false) {
    LOG(ERROR) << "Not a video pin.";
    return kNotVideo;
  }
  // Confirm that |pin| is connected.
  IPinPtr pin_peer;
  HRESULT hr = pin->ConnectedTo(&pin_peer);
  if (hr != S_OK || !pin_peer) {
    LOG(ERROR) << "pin not connected.";
    return kNotConnected;
  }
  pin_ = pin;
  return kSuccess;
}

// Grabs the pin media type, extracts the |VIDEOINFOHEADER|, calculates frame
// rate using |AvgTimePerFrame|, and returns the value.  Returns a rate <0 on
// failure. Note that an |AvgTimePerFrame| of 0 is interpreted as an error.
double VideoPinInfo::frame_rate() const {
  double frames_per_second = -1.0;
  // Validate |pin_| again; |_com_ptr_t| throws when empty.
  if (pin_) {
    AM_MEDIA_TYPE media_type;
    const HRESULT hr = pin_->ConnectionMediaType(&media_type);
    if (hr == S_OK) {
      VideoMediaType video_format;
      const int status = video_format.Init(media_type);
      if (status == kSuccess) {
        frames_per_second = video_format.frame_rate();
      }
      MediaType::FreeMediaTypeData(&media_type);
    }
  }
  return frames_per_second;
}

///////////////////////////////////////////////////////////////////////////////
// PinFormat
//

// Construct |PinFormat| for |pin|.
PinFormat::PinFormat(const IPinPtr& pin)
    : pin_(pin) {
}

PinFormat::~PinFormat() {
}

// Returns |pin_| format via use of IAMStreamConfig::GetFormat, or returns NULL
// on failure.
AM_MEDIA_TYPE* PinFormat::format() const {
  const IAMStreamConfigPtr config(pin_);
  if (!config) {
    LOG(ERROR) << "pin_ has no IAMStreamConfig interface.";
    return NULL;
  }
  AM_MEDIA_TYPE* ptr_current_format = NULL;
  const HRESULT hr = config->GetFormat(&ptr_current_format);
  if (FAILED(hr)) {
    LOG(ERROR) << "IAMStreamConfig::GetFormat failed: " << HRLOG(hr);
  }
  return ptr_current_format;
}

// Sets |pin_| format via use of IAMStreamConfig::SetFormat. Returns
// |kCannotSetFormat| on failure.
int PinFormat::set_format(const AM_MEDIA_TYPE* ptr_format) {
  // Note: NULL |ptr_format| is OK-- some filters treat a NULL format as a
  //       request to reset to the pin's default format.
  const IAMStreamConfigPtr config(pin_);
  if (!config) {
    LOG(ERROR) << "pin_ has no IAMStreamConfig interface.";
    return kCannotSetFormat;
  }
  const HRESULT hr = config->SetFormat(const_cast<AM_MEDIA_TYPE*>(ptr_format));
  if (FAILED(hr)) {
    LOG(ERROR) << "cannot set pin_ format: " << HRLOG(hr);
    return kCannotSetFormat;
  }
  return kSuccess;
}

// Returns AM_MEDIA_TYPE pointer with settings matching those requested, or
// NULL.
AM_MEDIA_TYPE* PinFormat::FindMatchingFormat(const AudioConfig& config) {
  IEnumMediaTypesPtr media_types;
  HRESULT hr = pin_->EnumMediaTypes(&media_types);
  if (FAILED(hr)) {
    LOG(ERROR) << "pin cannot enumerate media types.";
    return NULL;
  }
  MediaTypePtr format;
  for (;;) {
    hr = media_types->Next(1, format.GetPtr(), NULL);
    if (hr != S_OK) {
      LOG(INFO) << "exhausted audio media types without finding a match.";
      // be certain |format| is empty.
      format.Free();
      break;
    }
    AudioMediaType audio_format;
    const int status = audio_format.Init(*format.get());
    if (status) {
      LOG(INFO) << "skipping unsupported audio media type.";
      continue;
    }
    if (audio_format.channels() == config.channels &&
        audio_format.sample_rate() == config.sample_rate &&
        audio_format.bits_per_sample() == config.bits_per_sample) {
      LOG(INFO) << "Found matching audio media type.";
      break;
    }
  }
  return format.Detach();
}

// Returns AM_MEDIA_TYPE pointer with settings matching those requested, or
// NULL.
AM_MEDIA_TYPE* PinFormat::FindMatchingFormat(const VideoConfig& config) {
  IEnumMediaTypesPtr media_types;
  HRESULT hr = pin_->EnumMediaTypes(&media_types);
  if (FAILED(hr)) {
    LOG(ERROR) << "pin cannot enumerate media types.";
    return NULL;
  }
  MediaTypePtr format;
  for (;;) {
    hr = media_types->Next(1, format.GetPtr(), NULL);
    if (hr != S_OK) {
      LOG(INFO) << "exhausted video media types without finding a match.";
      // be certain |format| is empty.
      format.Free();
      break;
    }
    VideoMediaType video_format;
    const int status = video_format.Init(*format.get());
    if (status) {
      LOG(INFO) << "skipping unsupported video media type.";
      continue;
    }
    if (video_format.width() == config.width &&
        video_format.height() == config.height &&
        video_format.frame_rate() == config.frame_rate) {
      LOG(INFO) << "Found matching video media type.";
      break;
    }
  }
  return format.Detach();
}

// Displays |filter|'s property page.
HRESULT ShowFilterPropertyPage(const IBaseFilterPtr& filter) {
  if (!filter) {
    LOG(ERROR) << "empty IBaseFilterPtr.";
    return E_POINTER;
  }
  // Get |filter|'s IUnknown pointer.
  IUnknownPtr iunknown(filter);
  if (!iunknown) {
    LOG(ERROR) << "filter does not support IUnknown?!";
    return E_NOINTERFACE;
  }
  return ShowPropertyPage(iunknown.GetInterfacePtr());
}

// Displays |pin|'s property page.
HRESULT ShowPinPropertyPage(const IPinPtr& pin) {
  if (!pin) {
    LOG(ERROR) << "empty IPinPtr.";
    return E_POINTER;
  }
  // Get |pin|'s IUnknown pointer.
  IUnknownPtr iunknown(pin);
  if (!iunknown) {
    LOG(ERROR) << "pin does not support IUnknown?!";
    return E_NOINTERFACE;
  }
  return ShowPropertyPage(iunknown.GetInterfacePtr());
}

// Displays property page for |ptr_iunknown|, and returns HRESULT code from
// the windows COM API call.
HRESULT ShowPropertyPage(IUnknown* ptr_iunknown) {
  if (!ptr_iunknown) {
    LOG(ERROR) << "Null IUnknown pointer.";
    return E_POINTER;
  }
  ISpecifyPropertyPagesPtr prop(ptr_iunknown);
  if (!prop) {
    LOG(ERROR) << "ISpecifyPropertyPages not supported.";
    return E_NOINTERFACE;
  }
  // Show |ptr_iunknown|'s page.
  CAUUID caGUID;
  HRESULT hr = prop->GetPages(&caGUID);
  if (FAILED(hr)) {
    LOG(ERROR) << "filter has no property pages." << HRLOG(hr);
    return hr;
  }
  // Release |prop| because this is what the MSDN sample does.
  prop = 0;
  hr = OleCreatePropertyFrame(NULL,                // Parent window
                              0, 0,                // Reserved
                              NULL,                // Caption for the dialog.
                              1,                   // Number of objects.
                              &ptr_iunknown,
                              caGUID.cElems,       // Number of prop pages.
                              caGUID.pElems,       // Array of page CLSIDs.
                              0,                   // Locale identifier.
                              0, NULL);            // Reserved
  // Note: |OleCreatePropertyFrame| returns S_OK as long as the prop dialog is
  //       shown. It does not mean that the user did anything in particular.
  //       Log the return value if it isn't |S_OK|, because that would be
  //       weird.
  if (hr != S_OK) {
    LOG(INFO) << "OleCreatePropertyFrame returned: " << HRLOG(hr);
  }
  if (caGUID.pElems) {
    CoTaskMemFree(caGUID.pElems);
  }
  return hr;
}

}  // namespace webmlive
