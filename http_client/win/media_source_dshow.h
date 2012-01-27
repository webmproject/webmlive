// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef HTTP_CLIENT_WIN_MEDIA_SOURCE_DSHOW_H_
#define HTTP_CLIENT_WIN_MEDIA_SOURCE_DSHOW_H_

#include <comdef.h>
#include <dshow.h>

#include <map>
#include <string>

#include "boost/shared_ptr.hpp"
#include "boost/thread/thread.hpp"
#include "http_client/basictypes.h"
#include "http_client/http_client_base.h"
#include "http_client/webm_encoder.h"

namespace webmlive {
// A slightly more brief version of the com_ptr_t definition macro.
#define COMPTR_TYPEDEF(InterfaceName) \
  _COM_SMARTPTR_TYPEDEF(InterfaceName, IID_##InterfaceName)
COMPTR_TYPEDEF(IAMStreamConfig);
COMPTR_TYPEDEF(IBaseFilter);
COMPTR_TYPEDEF(ICaptureGraphBuilder2);
COMPTR_TYPEDEF(ICreateDevEnum);
COMPTR_TYPEDEF(IEnumMediaTypes);
COMPTR_TYPEDEF(IEnumMoniker);
COMPTR_TYPEDEF(IEnumPins);
COMPTR_TYPEDEF(IFilterGraph);
COMPTR_TYPEDEF(IGraphBuilder);
COMPTR_TYPEDEF(IMediaControl);
COMPTR_TYPEDEF(IMediaEvent);
COMPTR_TYPEDEF(IMediaSeeking);
COMPTR_TYPEDEF(IMoniker);
COMPTR_TYPEDEF(IPin);
COMPTR_TYPEDEF(IPropertyBag);
COMPTR_TYPEDEF(ISpecifyPropertyPages);


// CLSID constants for directshow filters needed to encode WebM files.
// Xiph.org Vorbis encoder CLSID
const CLSID CLSID_VorbisEncoder = {
  // 5C94FE86-B93B-467F-BFC3-BD6C91416F9B
  0x5C94FE86,
  0xB93B,
  0x467F,
  {0xBF, 0xC3, 0xBD, 0x6C, 0x91, 0x41, 0x6F, 0x9B}
};
// Webmdshow project color conversion filter CLSID.  Not used at present.
const CLSID CLSID_WebmColorConversion = {
  // ED311140-5211-11DF-94AF-0026B977EEAA
  0xED311140,
  0x5211,
  0x11DF,
  {0x94, 0xAF, 0x00, 0x26, 0xB9, 0x77, 0xEE, 0xAA}
};
// Webmdshow project muxer filter CLSID.
const CLSID CLSID_WebmMux = {
  // ED3110F0-5211-11DF-94AF-0026B977EEAA
  0xED3110F0,
  0x5211,
  0x11DF,
  {0x94, 0xAF, 0x00, 0x26, 0xB9, 0x77, 0xEE, 0xAA}
};
// Webmdshow project VP8 encoder filter CLSID.
const CLSID CLSID_VP8Encoder = {
  // ED3110F5-5211-11DF-94AF-0026B977EEAA
  0xED3110F5,
  0x5211,
  0x11DF,
  {0x94, 0xAF, 0x00, 0x26, 0xB9, 0x77, 0xEE, 0xAA}
};

// Utility functions for time conversions.
int64 media_time_to_milliseconds(REFERENCE_TIME media_time);
double media_time_to_seconds(REFERENCE_TIME media_time);
REFERENCE_TIME seconds_to_media_time(double seconds);

class PinInfo;
class VideoFrameCallbackInterface;

// Platform specific media source object. Currently supports only video.
//
// Captures video frames using a custom sink filter and passes them back to
// users through VideoFrameCallbackInterface.
class MediaSourceImpl {
 public:
  typedef WebmEncoderConfig::AudioCaptureConfig AudioConfig;
  typedef WebmEncoderConfig::VideoCaptureConfig VideoConfig;
  enum {
    // Error creating the video sink filter.
    kVideoSinkCreateError = -222,

    // Error configuring Vorbis encoder.
    kVorbisConfigureError = -221,

    // Unable to obtain Vorbis encoder configuration interface.
    kCannotConfigureVorbisEncoder = -220,

    // Graph abort event received.
    kGraphAborted = -219,

    // Unable to configure the graph.
    kGraphConfigureError = -215,

    // Unable to connect the file writer filter.
    kFileWriterConnectError = -214,

    // Unable to create the file writer filter.
    kCannotCreateFileWriter = -213,

    // Unable to connect Vorbis encoder to WebM muxer.
    kWebmMuxerAudioConnectError = -212,

    // Unable to connect VP8 encoder to WebM muxer.
    kWebmMuxerVideoConnectError = -211,

    // Error configuring WebM muxer.
    kWebmMuxerConfigureError = -210,

    // Unable to obtain WebM muxer configuration interface.
    kCannotConfigureWebmMuxer = -209,

    // Unable to connect audio source to Vorbis encoder.
    kAudioConnectError = -208,

    // Unable to connect video source to VP8 encoder.
    kVideoConnectError = -207,

    // Error configuring VP8 encoder.
    kVpxConfigureError = -206,

    // Unable to obtain VP8 encoder configuration interface.
    kCannotConfigureVpxEncoder = -205,

    // Unable to add a filter to the graph.
    kCannotAddFilter = -204,

    // Unable to create the Vorbis encoder filter.
    kCannotCreateVorbisEncoder = -203,

    // Unable to create the VP8 encoder filter.
    kCannotCreateVpxEncoder = -202,

    // Unable to create the WebM muxer filter.
    kCannotCreateWebmMuxer = -201,

    // Unable to create graph interfaces.
    kCannotCreateGraph = -200,

    kInvalidArg = -1,
    kSuccess = 0,

    // Graph completion event received.
    kGraphCompleted = 1,
  };
  MediaSourceImpl();
  ~MediaSourceImpl();

  // Creates video capture graph. Returns |kSuccess| upon success, or a
  // |WebmEncoder| status code upon failure.
  int Init(const WebmEncoderConfig& config,
           VideoFrameCallbackInterface* ptr_video_callback);

  // Runs filter graph. Returns |kSuccess| upon success, or a |WebmEncoder|
  // status code upon failure.
  int Run();

  // Monitors filter graph state.
  int CheckStatus();

  // Stops filter graph.
  void Stop();

  // Returns encoded duration in seconds.
  double encoded_duration();

  // Configuration accessors.
  AudioConfig requested_audio_config() const {
    return requested_audio_config_;
  };
  AudioConfig actual_audio_config() const {
    return actual_audio_config_;
  };
  VideoConfig requested_video_config() const {
    return requested_video_config_;
  };
  VideoConfig actual_video_config() const {
    return actual_video_config_;
  };

 private:
  // Creates filter graph and graph builder interfaces.
  int CreateGraph();

  // Creates video capture source filter instance and adds it to the graph.
  int CreateVideoSource();

  // Creates the video sink filter instance and adds it to the graph.
  int CreateVideoSink();

  // Connects the video source and sink filters.
  int ConnectVideoSourceToVideoSink();

  // Configures the video capture source using |sub_type| and
  // |config_.requested_video_config|.
  int ConfigureVideoSource(const IPinPtr& pin, int sub_type);

  // Obtains interfaces and data needed to monitor and control the graph.
  int InitGraphControl();

  // Copies |video_source_| to |audio_source_| if |video_source_| has an audio
  // output pin, or creates an audio capture source filter instance and adds
  // it to the graph.
  int CreateAudioSource();

  // Configures the audio capture source.
  int ConfigureAudioSource(const IPinPtr& pin);

  // Loads the Vorbis encoder and adds it to the graph.
  int CreateVorbisEncoder();

  // Connects audio source to Vorbis encoder.
  int ConnectAudioSourceToVorbisEncoder();

  // Configures the xiph.org Vorbis encoder filter.
  int ConfigureVorbisEncoder();

  // Checks graph media event for error or completion.
  int HandleMediaEvent();

  // Flag set to true when audio is captured from the same filter as video.
  bool audio_from_video_source_;

  // Handle to graph media event. Used to check for graph error and completion.
  HANDLE media_event_handle_;

  // Graph builder interfaces
  IGraphBuilderPtr graph_builder_;
  ICaptureGraphBuilder2Ptr capture_graph_builder_;

  // Directshow filters used in the encoder graph.
  IBaseFilterPtr audio_source_;
  IBaseFilterPtr video_source_;
  IBaseFilterPtr video_sink_;
  IBaseFilterPtr vorbis_encoder_;
  IBaseFilterPtr vpx_encoder_;

  // Graph control interface.
  IMediaControlPtr media_control_;

  // Media event interface used when |media_event_handle_| is signaled.
  IMediaEventPtr media_event_;

  // Audio device friendly name.
  std::wstring audio_device_name_;

  // Video device friendly name.
  std::wstring video_device_name_;

  // Requested audio settings.
  AudioConfig requested_audio_config_;

  // Actual audio settings.
  AudioConfig actual_audio_config_;

  // Requested video settings.
  VideoConfig requested_video_config_;

  // Actual video settings.
  VideoConfig actual_video_config_;

  // Callback interface used by video sink filter to deliver raw frames to
  // |WebmEncoder::EncoderThread|.
  VideoFrameCallbackInterface* ptr_video_callback_;
  WEBMLIVE_DISALLOW_COPY_AND_ASSIGN(MediaSourceImpl);
};

// Utility class for finding and loading capture devices available through
// DirectShow on user systems.
class CaptureSourceLoader {
 public:
  enum {
    kNoDeviceFound = -300,
    kSuccess = 0,
  };
  CaptureSourceLoader();
  ~CaptureSourceLoader();

  // Initialize the loader for audio or video devices.  Must specify either
  // CLSID_AudioInputDeviceCategory or CLSID_VideoInputDeviceCategory.
  int Init(CLSID source_type);

  // Returns number of sources found by Init.
  int GetNumSources() const { return sources_.size(); }

  // Return source name for specified index.
  std::wstring GetSourceName(int index) { return sources_[index]; }

  // Returns filter for capture source at specified |index|.
  IBaseFilterPtr GetSource(int index);

  // Returns filter for capture source specified by |name|.
  IBaseFilterPtr GetSource(const std::wstring name);

 private:
  // Finds and stores all source devices of |source_type_| in |sources_|.
  int FindAllSources();

  // Utility for returning the string property specified by |prop_name| stored
  // in |prop_bag|.
  std::wstring GetStringProperty(const IPropertyBagPtr& prop_bag,
                                 std::wstring prop_name);

  // Returns the value of |moniker|'s friendly name property.
  std::wstring GetMonikerFriendlyName(const IMonikerPtr& moniker);

  // Type of sources to find.
  CLSID source_type_;

  // System input device enumerator.
  IEnumMonikerPtr source_enum_;

  // Map of sources.
  std::map<int, std::wstring> sources_;
  WEBMLIVE_DISALLOW_COPY_AND_ASSIGN(CaptureSourceLoader);
};

// Utility class for finding a specific pin on a DirectShow filter.
class PinFinder {
 public:
  PinFinder();
  ~PinFinder();

  // Initialize pin finder.
  int Init(const IBaseFilterPtr& filter);

  // TODO(tomfinegan): generalize these with a FindPin that takes a comparator.

  // All Find methods return an empty IPinPtr if unsuccessful.
  // Returns audio input pin at index.
  IPinPtr FindAudioInputPin(int index) const;

  // Returns audio output pin at index.
  IPinPtr FindAudioOutputPin(int index) const;

  // Returns video input pin at index.
  IPinPtr FindVideoInputPin(int index) const;

  // Returns video output pin at index.
  IPinPtr FindVideoOutputPin(int index) const;

  // Returns stream input pint at index.
  IPinPtr FindStreamInputPin(int index) const;

  // Returns stream output pin at index.
  IPinPtr FindStreamOutputPin(int index) const;

  // Returns input pin at index.
  IPinPtr FindInputPin(int index) const;

 private:
  // Filter pin enumerator interface.
  IEnumPinsPtr pin_enum_;
  WEBMLIVE_DISALLOW_COPY_AND_ASSIGN(PinFinder);
};

// Utility class for obtaining information about a pin.
class PinInfo {
 public:
  enum {
    kCannotSetFormat = -1,
    kSuccess = 0,
  };
  // Copies supplied pin to |pin_|.
  explicit PinInfo(const IPinPtr& pin);
  ~PinInfo();

  // Checks for availability of specified major type.
  bool HasMajorType(GUID major_type) const;

  // Returns true for pins with media type audio.
  bool IsAudio() const;

  // Returns true for input pins.
  bool IsInput() const;

  // Returns true for output pins.
  bool IsOutput() const;

  // Returns true for pins with media type video.
  bool IsVideo() const;

  // Returns true for pins with media type stream.
  bool IsStream() const;

  // Returns |pin_|.
  IPinPtr pin() const { return pin_; }

 private:
  // Disallow construction without IPinPtr.
  PinInfo();

  // Copy of |ptr_pin| from |Init|
  const IPinPtr pin_;
  WEBMLIVE_DISALLOW_COPY_AND_ASSIGN(PinInfo);
};

// Utility class for obtaining video pin specific information.
class VideoPinInfo {
 public:
  enum {
    kNotConnected = -3,
    kNotVideo = -2,
    kInvalidArg = -1,
    kSuccess = 0,
  };
  VideoPinInfo();
  ~VideoPinInfo();

  // Copies |pin|, confirms that it's a video pin, and returns |kSuccess|.
  // Returns |kInvalidArg| if |pin| is empty, or |kNotVideo| if
  // |PinInfo::IsVideo| returns false. Note that |pin| *must* be connected:
  // |VideoPinInfo| uses |ConnectionMediaType|.
  int Init(const IPinPtr& pin);

  double frame_rate() const;

 private:
  IPinPtr pin_;
  WEBMLIVE_DISALLOW_COPY_AND_ASSIGN(VideoPinInfo);
};

// Utility class for accessing and manipulating pin media format.
class PinFormat {
 public:
  enum {
    kCannotSetFormat = -1,
    kSuccess = 0,
  };
  typedef WebmEncoderConfig::AudioCaptureConfig AudioConfig;
  typedef WebmEncoderConfig::VideoCaptureConfig VideoConfig;

  // Copies supplied pin to |pin_|.
  explicit PinFormat(const IPinPtr& pin);
  ~PinFormat();

  // Returns |pin_|'s current format, or NULL on failure. Caller must dispose
  // of pointer returned.
  AM_MEDIA_TYPE* format() const;

  // Attempts to set |pin_|'s format.
  int set_format(const AM_MEDIA_TYPE* ptr_format);

  // Enumerates pin media types searching for one that matches |config|.
  // Returns a NULL pointer when available types are exhausted without finding
  // a suitable match.
  AM_MEDIA_TYPE* FindMatchingFormat(const AudioConfig& config);
  AM_MEDIA_TYPE* FindMatchingFormat(const VideoConfig& config);

  // Returns |pin_|.
  IPinPtr pin() const { return pin_; }

 private:
  // Disallow construction without IPinPtr.
  PinFormat();

  // Copy of |ptr_pin| from |Init|
  const IPinPtr pin_;
  WEBMLIVE_DISALLOW_COPY_AND_ASSIGN(PinFormat);
};

// Returns result of |ShowPropertyPage| after obtaining IUnknown interface
// from |filter|.
HRESULT ShowFilterPropertyPage(const IBaseFilterPtr& filter);

// Returns result of |ShowPropertyPage| after obtaining IUnknown interface
// from |pin|.
HRESULT ShowPinPropertyPage(const IPinPtr& pin);

// Returns HRESULT code from attempt to show property page for |ptr_iunknown|.
HRESULT ShowPropertyPage(IUnknown* ptr_iunknown);

}  // namespace webmlive

#endif  // HTTP_CLIENT_WIN_MEDIA_SOURCE_DSHOW_H_
