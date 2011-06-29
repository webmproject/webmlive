// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef WEBMLIVE_FILE_READER_H
#define WEBMLIVE_FILE_READER_H

#pragma once

#include <map>
#include <string>

#include <comdef.h>
// files included by dshow.h cause many 4996 warnings
#pragma warning(disable:4996)
#include <dshow.h>
#pragma warning(default:4996)

#include "boost/shared_ptr.hpp"
#include "boost/thread/thread.hpp"
#include "chromium/base/basictypes.h"
#include "webm_encoder.h"

namespace WebmLive {

#define COMPTR_TYPEDEF(InterfaceName) \
  _COM_SMARTPTR_TYPEDEF(InterfaceName, IID_##InterfaceName)
COMPTR_TYPEDEF(IBaseFilter);
COMPTR_TYPEDEF(ICaptureGraphBuilder2);
COMPTR_TYPEDEF(ICreateDevEnum);
COMPTR_TYPEDEF(IEnumMediaTypes);
COMPTR_TYPEDEF(IEnumMoniker);
COMPTR_TYPEDEF(IEnumPins);
COMPTR_TYPEDEF(IFilterGraph);
COMPTR_TYPEDEF(IFileSinkFilter2);
COMPTR_TYPEDEF(IGraphBuilder);
COMPTR_TYPEDEF(IMoniker);
COMPTR_TYPEDEF(IPin);
COMPTR_TYPEDEF(IPropertyBag);

const CLSID CLSID_VorbisEncoder =
{ // 5C94FE86-B93B-467F-BFC3-BD6C91416F9B
  0x5C94FE86,
  0xB93B,
  0x467F,
  { 0xBF, 0xC3, 0xBD, 0x6C, 0x91, 0x41, 0x6F, 0x9B}
};

const CLSID CLSID_WebmColorConversion =
{ // ED311140-5211-11DF-94AF-0026B977EEAA
  0xED311140,
  0x5211,
  0x11DF,
  {0x94, 0xAF, 0x00, 0x26, 0xB9, 0x77, 0xEE, 0xAA}
};

const CLSID CLSID_WebmMux =
{ // ED3110F0-5211-11DF-94AF-0026B977EEAA
  0xED3110F0,
  0x5211,
  0x11DF,
  {0x94, 0xAF, 0x00, 0x26, 0xB9, 0x77, 0xEE, 0xAA}
};

const CLSID CLSID_VP8Encoder =
{ // ED3110F5-5211-11DF-94AF-0026B977EEAA */
  0xED3110F5,
  0x5211,
  0x11DF,
  {0x94, 0xAF, 0x00, 0x26, 0xB9, 0x77, 0xEE, 0xAA}
};

const IID IID_IVP8Encoder =
{ // ED3110FE-5211-11DF-94AF-0026B977EEAA
  0xED3110FE,
  0x5211,
  0x11DF,
  {0x94, 0xAF, 0x00, 0x26, 0xB9, 0x77, 0xEE, 0xAA}
};

class WebmEncoderImpl {
 public:
  enum {
    kVideoConnectError = -207,
    kVpxConfigureError = -206,
    kCannotConfigureVpxEncoder = -205,
    kCannotAddFilter = -204,
    kCannotCreateVorbisEncoder = -203,
    kCannotCreateVpxEncoder = -202,
    kCannotCreateWebmMuxer = -201,
    kCannotCreateGraph = -200,
    kSuccess = 0,
  };
  WebmEncoderImpl();
  ~WebmEncoderImpl();
  int Init(std::wstring out_file_name);
  int Run();
  int Stop();
 private:
  int CreateGraph();
  int CreateVideoSource(std::wstring video_src);
  int CreateVpxEncoder();
  int ConnectVideoSourceToVpxEncoder();
  int CreateAudioSource(std::wstring video_src);
  int CreateAudioEncoder();
  int ConnectAudioSourceToVorbisEncoder();
  int ConnectEncodersToWebmMuxer();
  int ConnectFileWriter();
  void WebmEncoderThread();

  IGraphBuilderPtr graph_builder_;
  ICaptureGraphBuilder2Ptr capture_graph_builder_;
  IBaseFilterPtr audio_source_;
  IBaseFilterPtr video_source_;
  IBaseFilterPtr vorbis_encoder_;
  IBaseFilterPtr vpx_encoder_;
  IBaseFilterPtr webm_muxer_;
  IFileSinkFilter2Ptr file_writer_;
  boost::shared_ptr<boost::thread> upload_thread_;
  std::wstring out_file_name_;
  DISALLOW_COPY_AND_ASSIGN(WebmEncoderImpl);
};

class CaptureSourceLoader {
 public:
  enum {
    kNoDeviceFound = -300,
    kSuccess = 0,
  };
  CaptureSourceLoader();
  ~CaptureSourceLoader();
  int Init(CLSID source_type);
  int GetNumSources() const { return sources_.size(); };
  std::wstring GetSourceName(int index) { return sources_[index]; };
  IBaseFilterPtr GetSource(int index);
 private:
  int FindAllSources();
  std::wstring GetStringProperty(IPropertyBagPtr& prop_bag,
                                 std::wstring prop_name);
  CLSID source_type_;
  IEnumMonikerPtr source_enum_;
  std::map<int, std::wstring> sources_;
  DISALLOW_COPY_AND_ASSIGN(CaptureSourceLoader);
};

class PinFinder {
 public:
  PinFinder();
  ~PinFinder();
  int Init(IBaseFilterPtr& filter);
  IPinPtr FindAudioInputPin(int index) const;
  IPinPtr FindAudioOutputPin(int index) const;
  IPinPtr FindVideoInputPin(int index) const;
  IPinPtr FindVideoOutputPin(int index) const;
 private:
  IEnumPinsPtr pin_enum_;
  DISALLOW_COPY_AND_ASSIGN(PinFinder);
};

class PinInfo {
 public:
  explicit PinInfo(IPinPtr& ptr_pin);
  ~PinInfo();
  bool HasMajorType(GUID major_type) const;
  bool IsAudio() const;
  bool IsInput() const;
  bool IsOutput() const;
  bool IsVideo() const;
 private:
  PinInfo();
  IPinPtr& pin_;
  DISALLOW_COPY_AND_ASSIGN(PinInfo);
};

} // WebmLive

#endif // WEBMLIVE_FILE_READER_H
