#pragma once

#include <atlbase.h>
#include <dshow.h>
#include <iostream>
#include <sstream>
#include <cassert>

#include "livehttpposter.hpp"
#include "runningobjecttable.hpp"
#include "cmdline.hpp"

using namespace std;

namespace live_webm_streamer
{

const CLSID CLSID_VP8Encoder = 
  { 0xED3110F5, 0x5211, 0x11DF, { 0x94, 0xAF, 0x00, 0x26, 0xB9, 0x77, 0xEE, 0xAA}};

const CLSID CLSID_WebmMux = 
  { 0xED3110F0, 0x5211, 0x11DF, { 0x94, 0xAF, 0x00, 0x26, 0xB9, 0x77, 0xEE, 0xAA}};
    
const CLSID CLSID_YUV12 =
  { 0xF2D86B56, 0xC675, 0x11DA, { 0x97, 0xad, 0x00, 0x50, 0x8d, 0xef, 0x94, 0xa4}};
    
const IID IID_IVP8Encoder = 
  { 0xED3110FE, 0x5211, 0x11DF, { 0x94, 0xAF, 0x00, 0x26, 0xB9, 0x77, 0xEE, 0xAA}};
    
//{5C94FE86-B93B-467F-BFC3-BD6C91416F9B}
const CLSID CLSID_VorbisEncoder =    
  { 0x5C94FE86, 0xB93B, 0x467F, { 0xBF, 0xC3, 0xBD, 0x6C, 0x91, 0x41, 0x6F, 0x9B}};
  
#define CHECK_HR(Y) \
{\
    HRESULT hr; \
    if(!SUCCEEDED(hr=(Y))) \
   {\
    std::cout<<"\n"<<"\t"<<#Y<<"\n";\
    wchar_t message[1024];\
    wsprintf(message, (L"HRESULT Code : %08X ==> "), hr);\
    AMGetErrorText(hr, message + lstrlen(message), 256);\
    std::wstring errorMessage;\
    std::wstringstream ss;\
    errorMessage.assign(message);\
    std::wcout <<"\t " << errorMessage.c_str() << std::endl;\
    return hr;\
   }\
}\


class FilterGraph
{
private:
	FilterGraph(const FilterGraph&);
	FilterGraph operator=(const FilterGraph);

public:
	FilterGraph();	
	~FilterGraph();   

  bool Parse(int argc, wchar_t* argv[]);
	HRESULT SetUp();
	HRESULT Run();
  HRESULT GetCaptureSource(IBaseFilter** source);    
  HRESULT GetAudioCaptureSource(IBaseFilter** source, int* channels, int* sample_rate, int* bps);
  HRESULT SetAudioCaptureFormat(IBaseFilter* audio_source, int* channels, int* sample_rate, int* bps);
  HRESULT RenderAudio(IBaseFilter* webm_mux);  
	HRESULT FindFilterInCategory(IBaseFilter** src, const IID& cls, std::wstring& filter_name, int device_num);
	HRESULT SetVideoCaptureFormat(IBaseFilter* source);
	HRESULT ConvertGuidToString(const GUID& sub_type, char* four_cc) const;

private:
	CComPtr<IGraphBuilder> graph_builder_;
	CComPtr<IMediaEvent> media_event_;
	CComPtr<IMediaControl> media_control_;
	CComPtr<ICaptureGraphBuilder2> capture_graph_builder2_;
	CComPtr<IBaseFilter> source_, vp8_encoder_, webm_mux_, file_writer_, yuv12_, render_, vorbis_encoder_;
	CComPtr<IFileSinkFilter2> file_sink_filter2_;
	
	RunningObjectTable monitor_running_graph;
	LivePoster live_http_poster_;
	//CmdLine cmd_line_;
	
	wchar_t* server_ip_;
	wchar_t* port_num_;
	wchar_t* out_webm_file_;
	
};

} // end of namespace