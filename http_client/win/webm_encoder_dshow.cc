// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "win/webm_encoder_dshow.h"

#include <dvdmedia.h>  // Needed for |VIDEOINFOHEADER2|.
#include <vfwmsgs.h>

#include <sstream>

#include "debug_util.h"
#include "webm_encoder.h"
#include "webmdshow/common/hrtext.hpp"
#include "webmdshow/IDL/vp8encoderidl.h"
#include "webmdshow/IDL/webmmuxidl.h"

namespace webmlive {

namespace {
// DirectShow Filter name constants.
const wchar_t* const kVideoSourceName = L"VideoSource";
const wchar_t* const kAudioSourceName = L"AudioSource";
const wchar_t* const kVpxEncoderName =  L"VP8Encoder";
const wchar_t* const kVorbisEncoderName = L"VorbisEncoder";
const wchar_t* const kWebmMuxerName = L"WebmMuxer";
const wchar_t* const kFileWriterName = L"FileWriter";

// Converts media time (100 nanosecond ticks) to seconds.
double media_time_to_seconds(REFERENCE_TIME media_time) {
  return media_time / 10000000.0;
}

// Converts seconds to media time (100 nanosecond ticks).
REFERENCE_TIME seconds_to_media_time(double seconds) {
  return static_cast<REFERENCE_TIME>(seconds * 10000000);
}

// Converts a std::string to std::wstring.
std::wstring string_to_wstring(std::string str) {
  std::wostringstream wstr;
  wstr << str.c_str();
  return wstr.str();
}
}  // anonymous namespace

WebmEncoderImpl::WebmEncoderImpl()
    : stop_(false),
      encoded_duration_(0.0),
      media_event_handle_(INVALID_HANDLE_VALUE) {
}

WebmEncoderImpl::~WebmEncoderImpl() {
  // Manually release directshow interfaces to avoid problems related to
  // destruction order of com_ptr_t members.
  file_writer_ = 0;
  webm_muxer_ = 0;
  vorbis_encoder_ = 0;
  audio_source_ = 0;
  vpx_encoder_ = 0;
  video_source_ = 0;
  media_event_handle_ = INVALID_HANDLE_VALUE;
  media_control_ = 0;
  media_event_ = 0;
  media_seeking_ = 0;
  capture_graph_builder_ = 0;
  graph_builder_ = 0;
  CoUninitialize();
}

// Build the DirectShow filter graph.
// Constructs a graph that looks like this:
// video source -> vp8 encoder    ->
//                                   webm muxer -> file writer
// audio source -> vorbis encoder ->
int WebmEncoderImpl::Init(const WebmEncoderConfig& config) {
  config_ = config;
  const HRESULT hr = CoInitialize(NULL);
  if (FAILED(hr)) {
    DBGLOG("CoInitialize failed: " << HRLOG(hr));
    return WebmEncoder::kInitFailed;
  }
  int status = CreateGraph();
  if (status) {
    DBGLOG("CreateGraphInterfaces failed: " << status);
    return WebmEncoder::kInitFailed;
  }
  if (!config.video_device_name.empty()) {
    video_device_name_ = string_to_wstring(config_.video_device_name);
  }
  status = CreateVideoSource();
  if (status) {
    DBGLOG("CreateVideoSource failed: " << status);
    return WebmEncoder::kNoVideoSource;
  }
  status = CreateVpxEncoder();
  if (status) {
    DBGLOG("CreateVpxEncoder failed: " << status);
    return WebmEncoder::kVideoEncoderError;
  }
  status = ConnectVideoSourceToVpxEncoder();
  if (status) {
    DBGLOG("ConnectVideoSourceToVpxEncoder failed: " << status);
    return WebmEncoder::kVideoEncoderError;
  }
  status = ConfigureVpxEncoder();
  if (status) {
    DBGLOG("ConfigureVpxEncoder failed: " << status);
    return WebmEncoder::kVideoEncoderError;
  }
  if (!config.audio_device_name.empty()) {
    audio_device_name_ = string_to_wstring(config_.audio_device_name);
  }
  status = CreateAudioSource();
  if (status) {
    DBGLOG("CreateAudioSource failed: " << status);
    return WebmEncoder::kNoAudioSource;
  }
  status = CreateVorbisEncoder();
  if (status) {
    DBGLOG("CreateVorbisEncoder failed: " << status);
    return WebmEncoder::kAudioEncoderError;
  }
  status = ConnectAudioSourceToVorbisEncoder();
  if (status) {
    DBGLOG("ConnectAudioSourceToVorbisEncoder failed: " << status);
    return WebmEncoder::kAudioEncoderError;
  }
  status = CreateWebmMuxer();
  if (status) {
    DBGLOG("CreateWebmMuxer failed: " << status);
    return WebmEncoder::kWebmMuxerError;
  }
  status = ConnectEncodersToWebmMuxer();
  if (status) {
    DBGLOG("ConnectEncodersToWebmMuxer failed: " << status);
    return WebmEncoder::kWebmMuxerError;
  }
  out_file_name_ = config.output_file_name;
  status = CreateFileWriter();
  if (status) {
    DBGLOG("CreateFileWriter failed: " << status);
    return WebmEncoder::kFileWriteError;
  }
  status = ConnectWebmMuxerToFileWriter();
  if (status) {
    DBGLOG("ConnectWebmMuxerToFileWriter failed: " << status);
    return WebmEncoder::kFileWriteError;
  }
  return kSuccess;
}

// Creates graph control and observation interfaces:
// - |media_control_|
// - |media_event_| (and media event handle: |media_event_handle_|)
// - |media_seeking_|
// Then starts the encoder thread.
int WebmEncoderImpl::Run() {
  if (encode_thread_) {
    DBGLOG("ERROR: non-null encode thread. Already running?");
    return WebmEncoder::kRunFailed;
  }
  media_control_ = IMediaControlPtr(graph_builder_);
  if (!media_control_) {
    DBGLOG("ERROR: cannot create media control.");
    return WebmEncoder::kEncodeControlError;
  }
  media_event_ = IMediaEventPtr(graph_builder_);
  if (!media_event_) {
    DBGLOG("ERROR: cannot create media event.");
    return WebmEncoder::kEncodeMonitorError;
  }
  OAEVENT* const ptr_handle = reinterpret_cast<OAEVENT*>(&media_event_handle_);
  const HRESULT hr = media_event_->GetEventHandle(ptr_handle);
  if (FAILED(hr)) {
    DBGLOG("ERROR: could not media event handle!" << HRLOG(hr));
    return WebmEncoder::kEncodeMonitorError;
  }
  media_seeking_ = IMediaSeekingPtr(graph_builder_);
  if (!media_seeking_) {
    DBGLOG("ERROR: cannot create media seeking interface.");
    return WebmEncoder::kEncodeMonitorError;
  }
  using boost::bind;
  using boost::shared_ptr;
  using boost::thread;
  using std::nothrow;
  encode_thread_ = shared_ptr<thread>(
      new (nothrow) thread(bind(&WebmEncoderImpl::WebmEncoderThread, this)));
  return kSuccess;
}

// Obtains lock on |mutex_|, sets |stop_| to true, and waits for the encode
// thread to finish via call to |join| on |encode_thread_|.
void WebmEncoderImpl::Stop() {
  assert(encode_thread_);
  boost::mutex::scoped_lock lock(mutex_);
  stop_ = true;
  lock.unlock();
  encode_thread_->join();
}

// Obtains lock on |mutex_| and returns |encoded_duration_|.
double WebmEncoderImpl::encoded_duration() {
  boost::mutex::scoped_lock lock(mutex_);
  return encoded_duration_;
}

// Tries to obtain lock on |mutex_| and returns value of |stop_| if lock is
// obtained. Assumes no stop requested and returns false if unable to obtain
// the lock.
bool WebmEncoderImpl::StopRequested() {
  bool stop_requested = false;
  boost::mutex::scoped_try_lock lock(mutex_);
  if (lock.owns_lock()) {
    stop_requested = stop_;
  }
  return stop_requested;
}

// Creates the graph builder, |graph_builder_|, and capture graph builder,
// |capture_graph_builder_|, and passes |graph_builder_| to
// |capture_graph_builder_|.
int WebmEncoderImpl::CreateGraph() {
  HRESULT hr = graph_builder_.CreateInstance(CLSID_FilterGraph);
  if (FAILED(hr)) {
    DBGLOG("ERROR: graph builder creation failed." << HRLOG(hr));
    return kCannotCreateGraph;
  }
  hr = capture_graph_builder_.CreateInstance(CLSID_CaptureGraphBuilder2);
  if (FAILED(hr)) {
    DBGLOG("ERROR: capture graph builder creation failed." << HRLOG(hr));
    return kCannotCreateGraph;
  }
  hr = capture_graph_builder_->SetFiltergraph(graph_builder_);
  if (FAILED(hr)) {
    DBGLOG("ERROR: could not set capture builder graph." << HRLOG(hr));
    return kGraphConfigureError;
  }
  return kSuccess;
}

// Uses |CaptureSourceLoader| to find a video capture source.  If successful
// an instance of the source filter is created and added to the filter graph.
// Note: the first device found is used unconditionally.
int WebmEncoderImpl::CreateVideoSource() {
  CaptureSourceLoader loader;
  int status = loader.Init(CLSID_VideoInputDeviceCategory);
  if (status) {
    DBGLOG("ERROR: no video source!");
    return WebmEncoder::kNoVideoSource;
  }
  for (int i = 0; i < loader.GetNumSources(); ++i) {
    DBGLOG("[" << i+1 << "] " << loader.GetSourceName(i).c_str());
  }
  if (video_device_name_.empty()) {
    video_source_ = loader.GetSource(0);
  } else {
    video_source_ = loader.GetSource(video_device_name_);
  }
  if (!video_source_) {
    DBGLOG("ERROR: cannot create video source!");
    return WebmEncoder::kNoVideoSource;
  }
  const HRESULT hr = graph_builder_->AddFilter(video_source_,
                                               kVideoSourceName);
  if (FAILED(hr)) {
    DBGLOG("ERROR: cannot add video source to graph." << HRLOG(hr));
    return kCannotAddFilter;
  }
  // TODO(tomfinegan): set video format instead of hoping for sane defaults.
  return kSuccess;
}

// Creates the VP8 encoder filter and adds it to the graph.
int WebmEncoderImpl::CreateVpxEncoder() {
  HRESULT hr = vpx_encoder_.CreateInstance(CLSID_VP8Encoder);
  if (FAILED(hr)) {
    DBGLOG("ERROR: VP8 encoder creation failed." << HRLOG(hr));
    return kCannotCreateVpxEncoder;
  }
  hr = graph_builder_->AddFilter(vpx_encoder_, kVpxEncoderName);
  if (FAILED(hr)) {
    DBGLOG("ERROR: cannot add VP8 encoder to graph." << HRLOG(hr));
    return kCannotAddFilter;
  }
  return kSuccess;
}

// Locates the output pin on |video_source_| and the input pin on
// |vpx_encoder_|, connects them directly.
int WebmEncoderImpl::ConnectVideoSourceToVpxEncoder() {
  PinFinder pin_finder;
  int status = pin_finder.Init(video_source_);
  if (status) {
    DBGLOG("ERROR: cannot look for pins on video source!");
    return kVideoConnectError;
  }
  IPinPtr video_src_pin = pin_finder.FindVideoOutputPin(0);
  if (!video_src_pin) {
    DBGLOG("ERROR: cannot find output pin on video source!");
    return kVideoConnectError;
  }
  status = pin_finder.Init(vpx_encoder_);
  if (status) {
    DBGLOG("ERROR: cannot look for pins on video source!");
    return kVideoConnectError;
  }
  IPinPtr vpx_input_pin = pin_finder.FindVideoInputPin(0);
  if (!vpx_input_pin) {
    DBGLOG("ERROR: cannot find video input pin on VP8 encoder!");
    return kVideoConnectError;
  }
  // TODO(tomfinegan): Add WebM Color Conversion filter when |ConnectDirect|
  //                   fails here.
  const HRESULT hr = graph_builder_->ConnectDirect(video_src_pin,
                                                   vpx_input_pin,
                                                   NULL);
  if (FAILED(hr)) {
    DBGLOG("ERROR: cannot connect video source to VP8 encoder." << HRLOG(hr));
    return kVideoConnectError;
  }
  return kSuccess;
}

// Sets minimal settings required for a realtime encode.
int WebmEncoderImpl::ConfigureVpxEncoder() {
  _COM_SMARTPTR_TYPEDEF(IVP8Encoder, __uuidof(IVP8Encoder));
  IVP8EncoderPtr vp8_config(vpx_encoder_);
  if (!vp8_config) {
    DBGLOG("ERROR: cannot create VP8 encoder interface.");
    return kCannotConfigureVpxEncoder;
  }
  // Set minimal defaults for a live encode...
  HRESULT hr = vp8_config->SetDeadline(kDeadlineRealtime);
  if (FAILED(hr)) {
    DBGLOG("ERROR: cannot set VP8 encoder deadline." << HRLOG(hr));
    return kVpxConfigureError;
  }
  hr = vp8_config->SetEndUsage(kEndUsageCBR);
  if (FAILED(hr)) {
    DBGLOG("ERROR: cannot set VP8 encoder bitrate mode." << HRLOG(hr));
    return kVpxConfigureError;
  }
  const WebmEncoderConfig::VpxConfig& config = config_.vpx_config;
  hr = vp8_config->SetTargetBitrate(config.bitrate);
  if (FAILED(hr)) {
    DBGLOG("ERROR: cannot set VP8 encoder bitrate." << HRLOG(hr));
    return kVpxConfigureError;
  }
  // Set keyframe interval.
  hr = vp8_config->SetKeyframeMode(kKeyframeModeDisabled);
  if (FAILED(hr)) {
    DBGLOG("ERROR: cannot set VP8 keyframe mode." << HRLOG(hr));
    return kVpxConfigureError;
  }
  const REFERENCE_TIME keyframe_interval =
      seconds_to_media_time(config.keyframe_interval);
  hr = vp8_config->SetFixedKeyframeInterval(keyframe_interval);
  if (FAILED(hr)) {
    DBGLOG("ERROR: cannot set VP8 keyframe interval." << HRLOG(hr));
    return kVpxConfigureError;
  }
  hr = vp8_config->SetMinQuantizer(config.min_quantizer);
  if (FAILED(hr)) {
    DBGLOG("ERROR: cannot set VP8 min quantizer value." << HRLOG(hr));
    return kVpxConfigureError;
  }
  hr = vp8_config->SetMaxQuantizer(config.max_quantizer);
  if (FAILED(hr)) {
    DBGLOG("ERROR: cannot set VP8 max quantizer value." << HRLOG(hr));
    return kVpxConfigureError;
  }
  if (config.speed != kUseEncoderDefault) {
    hr = vp8_config->SetCPUUsed(config.speed);
    if (FAILED(hr)) {
      DBGLOG("ERROR: cannot set VP8 speed (CPU used) value." << HRLOG(hr));
      return kVpxConfigureError;
    }
  }
  if (config.static_threshold != kUseEncoderDefault) {
    hr = vp8_config->SetStaticThreshold(config.static_threshold);
    if (FAILED(hr)) {
      DBGLOG("ERROR: cannot set VP8 static threshold value." << HRLOG(hr));
      return kVpxConfigureError;
    }
  }
  if (config.thread_count != kUseEncoderDefault) {
    hr = vp8_config->SetThreadCount(config.thread_count);
    if (FAILED(hr)) {
      DBGLOG("ERROR: cannot set VP8 thread count." << HRLOG(hr));
      return kVpxConfigureError;
    }
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
int WebmEncoderImpl::CreateAudioSource() {
  // Check for an audio pin on the video source.
  // TODO(tomfinegan): We assume that the user wants to use the audio feed
  //                   exposed by the video capture source.  This behavior
  //                   should be configurable.
  PinFinder pin_finder;
  int status = pin_finder.Init(video_source_);
  if (status) {
    DBGLOG("ERROR: cannot check video source for audio pins!");
    return WebmEncoder::kInitFailed;
  }
  if (pin_finder.FindAudioOutputPin(0)) {
    // Use the video source filter audio output pin.
    DBGLOG("Using video source filter audio output pin.");
    audio_source_ = video_source_;
  } else {
    // The video source doesn't have an audio output pin. Find an audio
    // capture source.
    CaptureSourceLoader loader;
    status = loader.Init(CLSID_AudioInputDeviceCategory);
    if (status) {
      DBGLOG("ERROR: no audio source!");
      return WebmEncoder::kNoAudioSource;
    }
    for (int i = 0; i < loader.GetNumSources(); ++i) {
      DBGLOG("[" << i+1 << "] " << loader.GetSourceName(i).c_str());
    }
    if (audio_device_name_.empty()) {
      audio_source_ = loader.GetSource(0);
    } else {
      audio_source_ = loader.GetSource(audio_device_name_);
    }
    if (!audio_source_) {
      DBGLOG("ERROR: cannot create audio source!");
      return WebmEncoder::kNoAudioSource;
    }
    const HRESULT hr = graph_builder_->AddFilter(audio_source_,
                                                 kAudioSourceName);
    if (FAILED(hr)) {
      DBGLOG("ERROR: cannot add audio source to graph." << HRLOG(hr));
      return kCannotAddFilter;
    }
  }
  // TODO(tomfinegan): set audio format instead of hoping for sane defaults.
  return kSuccess;
}

// Creates an instance of the Xiph.org Vorbis encoder filter, and adds it to
// the filter graph.
int WebmEncoderImpl::CreateVorbisEncoder() {
  HRESULT hr = vorbis_encoder_.CreateInstance(CLSID_VorbisEncoder);
  if (FAILED(hr)) {
    DBGLOG("ERROR: Vorbis encoder creation failed." << HRLOG(hr));
    return kCannotCreateVorbisEncoder;
  }
  hr = graph_builder_->AddFilter(vorbis_encoder_, kVorbisEncoderName);
  if (FAILED(hr)) {
    DBGLOG("ERROR: cannot add Vorbis encoder to graph." << HRLOG(hr));
    return kCannotAddFilter;
  }
  // TODO(tomfinegan): add Vorbis encoder configuration.
  return kSuccess;
}

// Locates the output pin on |audio_source_| and the input pin on
// |vorbis_encoder_|, and connects them directly.
int WebmEncoderImpl::ConnectAudioSourceToVorbisEncoder() {
  PinFinder pin_finder;
  int status = pin_finder.Init(audio_source_);
  if (status) {
    DBGLOG("ERROR: cannot look for pins on audio source!");
    return kAudioConnectError;
  }
  IPinPtr audio_src_pin = pin_finder.FindAudioOutputPin(0);
  if (!audio_src_pin) {
    DBGLOG("ERROR: cannot find output pin on audio source!");
    return kAudioConnectError;
  }
  status = pin_finder.Init(vorbis_encoder_);
  if (status) {
    DBGLOG("ERROR: cannot look for pins on video source!");
    return kAudioConnectError;
  }
  IPinPtr vorbis_input_pin = pin_finder.FindAudioInputPin(0);
  if (!vorbis_input_pin) {
    DBGLOG("ERROR: cannot find audio input pin on Vorbis encoder!");
    return kAudioConnectError;
  }
  const HRESULT hr = graph_builder_->ConnectDirect(audio_src_pin,
                                                   vorbis_input_pin,
                                                   NULL);
  if (FAILED(hr)) {
    DBGLOG("ERROR: cannot connect audio source to Vorbis encoder."
           << HRLOG(hr));
    return kAudioConnectError;
  }
  return kSuccess;
}

// Creates the WebM muxer filter and adds it to the filter graph.
int WebmEncoderImpl::CreateWebmMuxer() {
  HRESULT hr = webm_muxer_.CreateInstance(CLSID_WebmMux);
  if (FAILED(hr)) {
    DBGLOG("ERROR: webm muxer creation failed." << HRLOG(hr));
    return kCannotCreateWebmMuxer;
  }
  hr = graph_builder_->AddFilter(webm_muxer_, kWebmMuxerName);
  if (FAILED(hr)) {
    DBGLOG("ERROR: cannot add webm muxer to graph." << HRLOG(hr));
    return kCannotAddFilter;
  }
  _COM_SMARTPTR_TYPEDEF(IWebmMux, __uuidof(IWebmMux));
  IWebmMuxPtr mux_config(webm_muxer_);
  if (!mux_config) {
    DBGLOG("ERROR: cannot create webm muxer interface.");
    return kCannotConfigureWebmMuxer;
  }
  hr = mux_config->SetMuxMode(kWebmMuxModeLive);
  if (FAILED(hr)) {
    DBGLOG("ERROR: cannot enable webm live mux mode." << HRLOG(hr));
    return kWebmMuxerConfigureError;
  }
  // TODO(tomfinegan): set writing app
  return kSuccess;
}

// Finds the output pins on the encoder filters, and connects them directly to
// the input pins on the WebM muxer filter.
int WebmEncoderImpl::ConnectEncodersToWebmMuxer() {
  PinFinder pin_finder;
  int status = pin_finder.Init(vpx_encoder_);
  if (status) {
    DBGLOG("ERROR: cannot look for pins on vpx encoder!");
    return kWebmMuxerVideoConnectError;
  }
  IPinPtr encoder_pin = pin_finder.FindVideoOutputPin(0);
  if (!encoder_pin) {
    DBGLOG("ERROR: cannot find video output pin on vpx encoder!");
    return kWebmMuxerVideoConnectError;
  }
  status = pin_finder.Init(webm_muxer_);
  if (status) {
    DBGLOG("ERROR: cannot look for pins on webm muxer!");
    return kWebmMuxerVideoConnectError;
  }
  IPinPtr muxer_pin = pin_finder.FindVideoInputPin(0);
  if (!muxer_pin) {
    DBGLOG("ERROR: cannot find video input pin on webm muxer!");
    return kWebmMuxerVideoConnectError;
  }
  HRESULT hr = graph_builder_->ConnectDirect(encoder_pin, muxer_pin, NULL);
  if (FAILED(hr)) {
    DBGLOG("ERROR: cannot connect vpx encoder to webm muxer!" << HRLOG(hr));
    return kWebmMuxerVideoConnectError;
  }
  status = pin_finder.Init(vorbis_encoder_);
  if (status) {
    DBGLOG("ERROR: cannot look for pins on vorbis encoder!");
    return kWebmMuxerAudioConnectError;
  }
  encoder_pin = pin_finder.FindAudioOutputPin(0);
  if (!encoder_pin) {
    DBGLOG("ERROR: cannot find audio output pin on vorbis encoder!");
    return kWebmMuxerAudioConnectError;
  }
  status = pin_finder.Init(webm_muxer_);
  if (status) {
    DBGLOG("ERROR: cannot look for pins on webm muxer!");
    return kWebmMuxerAudioConnectError;
  }
  muxer_pin = pin_finder.FindAudioInputPin(0);
  if (!muxer_pin) {
    DBGLOG("ERROR: cannot find audio input pin on webm muxer!");
    return kWebmMuxerAudioConnectError;
  }
  hr = graph_builder_->ConnectDirect(encoder_pin, muxer_pin, NULL);
  if (FAILED(hr)) {
    DBGLOG("ERROR: cannot connect vorbis encoder to webm muxer!" << HRLOG(hr));
    return kWebmMuxerAudioConnectError;
  }
  return kSuccess;
}

// Creates the file writer filter, adds it to the graph, and sets the output
// file name.
int WebmEncoderImpl::CreateFileWriter() {
  HRESULT hr = file_writer_.CreateInstance(CLSID_FileWriter);
  if (FAILED(hr)) {
    DBGLOG("ERROR: file writer creation failed." << HRLOG(hr));
    return kCannotCreateFileWriter;
  }
  hr = graph_builder_->AddFilter(file_writer_, kFileWriterName);
  if (FAILED(hr)) {
    DBGLOG("ERROR: cannot add file writer to graph." << HRLOG(hr));
    return kCannotAddFilter;
  }
  IFileSinkFilter2Ptr writer_config(file_writer_);
  if (!writer_config) {
    DBGLOG("ERROR: cannot create file writer sink interface.");
    return kCannotCreateFileWriter;
  }
  std::wostringstream out_file_name_stream;
  out_file_name_stream << out_file_name_.c_str();
  hr = writer_config->SetFileName(out_file_name_stream.str().c_str(), NULL);
  if (FAILED(hr)) {
    DBGLOG("ERROR: cannot set output file name." << HRLOG(hr));
    return kCannotCreateFileWriter;
  }
  return kSuccess;
}

// Locates the output pin on |webm_muxer_|, and connects it directly to the
// input pin on |file_writer_|.
int WebmEncoderImpl::ConnectWebmMuxerToFileWriter() {
  PinFinder pin_finder;
  int status = pin_finder.Init(webm_muxer_);
  if (status) {
    DBGLOG("ERROR: cannot look for pins on vpx encoder!");
    return kFileWriterConnectError;
  }
  IPinPtr muxer_pin = pin_finder.FindStreamOutputPin(0);
  if (!muxer_pin) {
    DBGLOG("ERROR: cannot find stream output pin on webm muxer!");
    return kFileWriterConnectError;
  }
  status = pin_finder.Init(file_writer_);
  if (status) {
    DBGLOG("ERROR: cannot look for pins on webm muxer!");
    return kFileWriterConnectError;
  }
  IPinPtr writer_pin = pin_finder.FindInputPin(0);
  if (!writer_pin) {
    DBGLOG("ERROR: cannot find stream input pin on file writer!");
    return kFileWriterConnectError;
  }
  const HRESULT hr = graph_builder_->ConnectDirect(muxer_pin, writer_pin,
                                                   NULL);
  if (FAILED(hr)) {
    DBGLOG("ERROR: cannot connect webm muxer to file writer!" << HRLOG(hr));
    return kFileWriterConnectError;
  }
  return kSuccess;
}

// Checks |media_event_handle_| and reads the event from |media_event_| when
// signaled.  Responds only to completion and error events.
int WebmEncoderImpl::HandleMediaEvent() {
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
        DBGLOG("EC_ERRORABORT");
        status = kGraphAborted;
      } else if (code == EC_ERRORABORTEX) {
        DBGLOG("EC_ERRORABORTEX");
        status = kGraphAborted;
      } else if (code == EC_USERABORT) {
        DBGLOG("EC_USERABORT");
        status = kGraphAborted;
      } else if (code == EC_COMPLETE) {
        DBGLOG("EC_COMPLETE");
        status = kGraphCompleted;
      }
    } else {
      // Couldn't get the event; tell caller to abort.
      DBGLOG("GetEvent failed: " << HRLOG(hr));
      status = kGraphAborted;
    }
  }
  return status;
}

// Obtain the lock on |mutex_| and update |encoded_duration_|.
void WebmEncoderImpl::set_encoded_duration(double current_duration) {
  boost::mutex::scoped_lock lock(mutex_);
  encoded_duration_ = current_duration;
}

// Encoder thread. Runs until one of the following is true:
// - |StopRequested| returns true.
// - |HandleMediaEvent| receives an error or completion event.
void WebmEncoderImpl::WebmEncoderThread() {
  CoInitialize(NULL);
  HRESULT hr = media_control_->Run();
  if (FAILED(hr)) {
    DBGLOG("media control Run failed, cannot run encode!" << HRLOG(hr));
  } else {
    for (;;) {
      int status = HandleMediaEvent();
      bool stop_event_recvd =
          (status == kGraphAborted || status == kGraphCompleted);
      if (stop_event_recvd || StopRequested()) {
        break;
      }
      REFERENCE_TIME current_duration = 0;
      hr = media_seeking_->GetCurrentPosition(&current_duration);
      if (SUCCEEDED(hr)) {
        set_encoded_duration(media_time_to_seconds(current_duration));
      }
      SwitchToThread();  // yield our time slice if another thread is waiting.
    }
    hr = media_control_->Stop();
    if (FAILED(hr)) {
      DBGLOG("media control Stop failed!" << HRLOG(hr));
    } else {
      DBGLOG("graph stopping." << HRLOG(hr));
    }
  }
  CoUninitialize();
  DBGLOG("Done.");
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
    DBGLOG("ERROR: unknown device category!");
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
    DBGLOG("ERROR: source enumerator creation failed." << HRLOG(hr));
    return kNoDeviceFound;
  }
  const DWORD kNoEnumFlags = 0;
  hr = sys_enum->CreateClassEnumerator(source_type_, &source_enum_,
                                       kNoEnumFlags);
  if (FAILED(hr) || hr == S_FALSE) {
    DBGLOG("ERROR: moniker creation failed (no devices)." << HRLOG(hr));
    return kNoDeviceFound;
  }
  int source_index = 0;
  for (;;) {
    IMonikerPtr source_moniker;
    hr = source_enum_->Next(1, &source_moniker, NULL);
    if (FAILED(hr) || hr == S_FALSE || !source_moniker) {
      DBGLOG("Done enumerating sources, found " << source_index << " sources.");
      break;
    }
    std::wstring name = GetMonikerFriendlyName(source_moniker);
    if (name.empty()) {
      DBGLOG("source=" << source_index << " has no name, skipping.");
      continue;
    }
    DBGLOG("source=" << source_index << " name=" << name.c_str());
    sources_[source_index] = name;
    ++source_index;
  }
  if (sources_.size() == 0) {
    DBGLOG("No devices found!");
    return kNoDeviceFound;
  }
  return kSuccess;
}

// Obtains source name for |index| and returns result of
// |GetSource(std::wstring)|.
IBaseFilterPtr CaptureSourceLoader::GetSource(int index) {
  if (static_cast<size_t>(index) >= sources_.size()) {
    DBGLOG("ERROR: " << index << " is not a valid source index");
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
    DBGLOG("ERROR: empty source name.");
    return NULL;
  }
  HRESULT hr = source_enum_->Reset();
  if (FAILED(hr)) {
    DBGLOG("ERROR: cannot reset source enumerator!" << HRLOG(hr));
    return NULL;
  }
  IMonikerPtr source_moniker;
  for (;;) {
    hr = source_enum_->Next(1, &source_moniker, NULL);
    if (FAILED(hr) || hr == S_FALSE || !source_moniker) {
      DBGLOG("ERROR: ran out of devices before reaching requested index!");
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
    DBGLOG("ERROR: cannot bind filter!" << HRLOG(hr));
  }
  return filter;
}

// Extracts a string value from a |VARIANT|.  Returns emptry string on failure.
std::wstring CaptureSourceLoader::GetStringProperty(IPropertyBagPtr &prop_bag,
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
        DBGLOG("moniker has no friendly name property, or it's empty.");
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
    DBGLOG("ERROR: NULL filter.");
    return WebmEncoder::kInvalidArg;
  }
  const HRESULT hr = filter->EnumPins(&pin_enum_);
  if (FAILED(hr)) {
    DBGLOG("ERROR: cannot enum filter pins!" << HRLOG(hr));
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
        FreeMediaTypeData(ptr_media_type);
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

// Utility function for proper clean up of resources owned by |AM_MEDIA_TYPE|
// pointers.
void PinInfo::FreeMediaTypeData(AM_MEDIA_TYPE* ptr_media_type) {
  if (ptr_media_type) {
    AM_MEDIA_TYPE& mt = *ptr_media_type;
    if (mt.cbFormat != 0) {
      CoTaskMemFree((PVOID)mt.pbFormat);
      mt.cbFormat = 0;
      mt.pbFormat = NULL;
    }
    if (mt.pUnk != NULL) {
      // |pUnk| should not be used, but because the Microsoft example code
      // has this section, it's included here-- leaking is never OK. (The
      // example also includes the note "pUnk should not be used").
      mt.pUnk->Release();
      mt.pUnk = NULL;
    }
  }
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
    DBGLOG("ERROR: empty pin.");
    return kInvalidArg;
  }
  PinInfo info(pin);
  if (info.IsVideo() == false) {
    DBGLOG("ERROR: Not a video pin.");
    return kNotVideo;
  }
  // Confirm that |pin| is connected.
  IPinPtr pin_peer;
  HRESULT hr = pin->ConnectedTo(&pin_peer);
  if (hr != S_OK || !pin_peer) {
    DBGLOG("ERROR: pin not connected.");
    return kNotConnected;
  }
  pin_ = pin;
  return kSuccess;
}

// Grabs the pin media type, extracts the |VIDEOINFOHEADER|, calculates frame
// rate using |AvgTimePerFrame|, and returns the value.  Returns a rate <0 on
// failure. Note that an |AvgTimePerFrame| of 0 is interpreted as an error.
double VideoPinInfo::frames_per_second() const {
  double frames_per_second = -1.0;
  // Validate |pin_| again; |_com_ptr_t| throws when empty.
  if (pin_) {
    AM_MEDIA_TYPE ptr_media_type;
    HRESULT hr = pin_->ConnectionMediaType(&ptr_media_type);
    if (hr == S_OK) {
      int64 media_time_ticks_per_frame = 0;
      const GUID& format = ptr_media_type.formattype;
      if (format == FORMAT_MPEGVideo || format == FORMAT_VideoInfo) {
        // |FORMAT_MPEGVideo| and |FORMAT_VideoInfo| use |VIDEOINFOHEADER|.
        VIDEOINFOHEADER* ptr_video_info_header =
            reinterpret_cast<VIDEOINFOHEADER*>(ptr_media_type.pbFormat);
        media_time_ticks_per_frame = ptr_video_info_header->AvgTimePerFrame;
      } else if (format == FORMAT_MPEG2Video || format == FORMAT_VideoInfo2) {
        // |FORMAT_MPEG2Video| and |FORMAT_VideoInfo2| use |VIDEOINFOHEADER2|.
        VIDEOINFOHEADER2* ptr_video_info_header =
            reinterpret_cast<VIDEOINFOHEADER2*>(ptr_media_type.pbFormat);
        media_time_ticks_per_frame = ptr_video_info_header->AvgTimePerFrame;
      }
      if (media_time_ticks_per_frame != 0) {
        const double seconds_per_frame =
            media_time_to_seconds(media_time_ticks_per_frame);
        frames_per_second = 1.0/seconds_per_frame;
      }
      PinInfo::FreeMediaTypeData(&ptr_media_type);
    }
  }
  return frames_per_second;
}

}  // namespace webmlive
