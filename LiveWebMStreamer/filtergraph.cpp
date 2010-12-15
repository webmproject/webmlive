#include <iomanip>
#include <iostream>
#include <fstream>
#include "FilterGraph.hpp"
#include "vp8encoderidl.h"
#include "webmmuxidl.h"

extern HANDLE g_handle_quit;

using namespace std;

namespace live_webm_streamer {

struct Parameters_
{
  HANDLE h;
  ifstream ifs;
};
  
FilterGraph::FilterGraph():
  graph_builder_(NULL),
  media_event_(NULL),
  media_control_(NULL),
  capture_graph_builder2_(NULL),
  source_(NULL),
  vp8_encoder_(NULL),
  webm_mux_(NULL),
  file_writer_(NULL),
  yuv12_(NULL),
  file_sink_filter2_(NULL),
  render_(NULL)
{
}

FilterGraph::~FilterGraph()
{
}

bool FilterGraph::Parse(int argc, wchar_t* argv[])
{
  const size_t len_ip = wcslen(argv[1]);
  assert(len_ip != 0);
  
  server_ip_ = new wchar_t[len_ip+1];
  wcscpy_s(server_ip_, len_ip+1, argv[1]);
  //TODO: handle server_ip with more verification

  const size_t len_port = wcslen(argv[2]);
  assert(len_port != 0);
  
  port_num_ = new wchar_t[len_port+1];
  wcscpy_s(port_num_, len_port+1, argv[2]);
  //TODO: handle port with more verification
  
  if (argc == 3)
  {
    const wchar_t default_webm_file[] = L"test.webm";
    
    const size_t len_webm_file = wcslen(default_webm_file);
    out_webm_file_ = new wchar_t[len_webm_file+1];
    wcscpy_s(out_webm_file_, len_webm_file+1, default_webm_file);
  }
  else if (argc == 4)
  {
    const size_t len_webm_file = wcslen(argv[3]);
    assert(len_webm_file != 0);
    
    out_webm_file_ = new wchar_t[len_webm_file+1];
    wcscpy_s(out_webm_file_, len_webm_file+1, argv[3]);
  }
  else
  {
    return true;
    // TODO(Hwasoo) : Handle here gracefully.
     ;
  }

  return false;
}

HRESULT FilterGraph::SetUp()
{
  CComPtr<IBaseFilter> source, audio_source;
  CComPtr<IVP8Encoder> vp8_interface;

  int channels = 0;
  int sample_rate = 0;
  int bps = 0;
  
  CHECK_HR(CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, IID_IGraphBuilder, reinterpret_cast<void**>(&graph_builder_)));
  CHECK_HR(CoCreateInstance(CLSID_CaptureGraphBuilder2, NULL, CLSCTX_INPROC_SERVER, IID_ICaptureGraphBuilder2, reinterpret_cast<void**>(&capture_graph_builder2_)));
  CHECK_HR(capture_graph_builder2_->SetFiltergraph(graph_builder_));
  CHECK_HR(GetCaptureSource(&source));
  CHECK_HR(GetAudioCaptureSource(&audio_source, &channels, &sample_rate, &bps));
  CHECK_HR(graph_builder_->AddFilter(source, L"Capture Vidoe Source"));
  CHECK_HR(graph_builder_->AddFilter(audio_source, L"Capture Audio Source"));
  CHECK_HR(vp8_encoder_.CoCreateInstance(CLSID_VP8Encoder));
  CHECK_HR(vorbis_encoder_.CoCreateInstance(CLSID_VorbisEncoder));
  CHECK_HR(graph_builder_->AddFilter(vp8_encoder_, L"VP8 Encoder"));
  CHECK_HR(graph_builder_->AddFilter(vorbis_encoder_, L"Vorbis Encoder"));
  CHECK_HR(vp8_encoder_->QueryInterface(IID_IVP8Encoder, reinterpret_cast<void**>(&vp8_interface)));
  CHECK_HR(vp8_interface->SetDeadline(1));
  CHECK_HR(yuv12_.CoCreateInstance(CLSID_YUV12));
  CHECK_HR(graph_builder_->AddFilter(yuv12_, L"YV12"));
  CHECK_HR(capture_graph_builder2_->RenderStream(&PIN_CATEGORY_CAPTURE, NULL, source, NULL, yuv12_));
  CHECK_HR(capture_graph_builder2_->RenderStream(NULL, NULL, yuv12_, NULL, vp8_encoder_));
  CHECK_HR(capture_graph_builder2_->RenderStream(&PIN_CATEGORY_CAPTURE, NULL, audio_source, NULL, vorbis_encoder_));
  CHECK_HR(CoCreateInstance(CLSID_VideoRenderer, NULL, CLSCTX_INPROC, IID_IBaseFilter, reinterpret_cast<void**>(&render_)));
  CHECK_HR(webm_mux_.CoCreateInstance(CLSID_WebmMux));
  CHECK_HR(graph_builder_->AddFilter(webm_mux_, L"WebM Mux"));
  CHECK_HR(capture_graph_builder2_->RenderStream(NULL ,NULL, vp8_encoder_, NULL, webm_mux_));
  CHECK_HR(capture_graph_builder2_->RenderStream(NULL ,NULL, vorbis_encoder_, NULL, webm_mux_));
  CHECK_HR(CoCreateInstance(CLSID_FileWriter, NULL, CLSCTX_INPROC, IID_IBaseFilter, reinterpret_cast<void**>(&file_writer_)));
  CHECK_HR(graph_builder_->AddFilter(file_writer_, L"File Writer"));
  CHECK_HR(file_writer_->QueryInterface(IID_IFileSinkFilter2, reinterpret_cast<void**>(&file_sink_filter2_)));
  CHECK_HR(file_sink_filter2_->SetMode(AM_FILE_OVERWRITE));
  //CHECK_HR(file_sink_filter2_->SetFileName(L"test.webm", NULL));
  CHECK_HR(file_sink_filter2_->SetFileName(out_webm_file_, NULL));
  CHECK_HR(capture_graph_builder2_->RenderStream(NULL, NULL, webm_mux_, NULL, file_writer_));

	return S_OK;
}

HRESULT FilterGraph::GetCaptureSource(IBaseFilter** source)
{
  int selected_device = 0;
  cout << "Video Capture Devices: " << endl;

  wstring filter_name;
  int i = 0;

  for (;;)
  {
	  CComPtr<IBaseFilter> temp_source;
	  HRESULT hr = FindFilterInCategory(&temp_source, CLSID_VideoInputDeviceCategory, filter_name, i);

	  if ((!SUCCEEDED(hr)) && (i == 0))
	  {
		  cout << " ERROR: No Video Capture device could be found" << endl;
		  return E_FAIL;
	  }
	  else if (!SUCCEEDED(hr))
		  break;

	  wcout << "    " << setw(3) << i << ") " << filter_name.c_str() << endl;
    ++i;
  }

  cout << "Please select a capture device (0-" << i-1 << "):";
  cin >> selected_device;

  if ((selected_device >= i) || ( selected_device < 0))
  {
    cout << endl << "ERROR:  No video capture device selected" << endl;
    return E_FAIL;
  }

  CHECK_HR(FindFilterInCategory(source, CLSID_VideoInputDeviceCategory, filter_name, selected_device));
  CHECK_HR(SetVideoCaptureFormat(*source));

  return S_OK;
}

HRESULT FilterGraph::SetVideoCaptureFormat(IBaseFilter* source)
{
	CComPtr<IAMStreamConfig> am_stream_config;
	CHECK_HR(capture_graph_builder2_->FindInterface(NULL, NULL, source, IID_IAMStreamConfig, reinterpret_cast<void**>(&am_stream_config)));

	int count = 0;
	int size = 0;
		
	CHECK_HR(am_stream_config->GetNumberOfCapabilities(&count, &size));
	assert(count >= 0);
  
	AM_MEDIA_TYPE* video_settings = NULL;
	VIDEO_STREAM_CONFIG_CAPS video_stream_config_caps;

	cout << "\n\n  Video capture formats:\n";

  int i;
	for (i = 0; i < count; ++i)
	{
		CHECK_HR(am_stream_config->GetStreamCaps(i, &video_settings, (BYTE*)&video_stream_config_caps)); 
		CHAR four_cc[10];
		ConvertGuidToString(video_settings->subtype, four_cc);
		cout << "    " << setw(3) << i << ") " << setw(6) << four_cc << " - " << video_stream_config_caps.InputSize.cx\
		  << " X " << video_stream_config_caps.InputSize.cy << setfill(' ') << endl;
	}

  int selected_format;
	cout << "  Please Select a format (0-" << i-1 << "):";
	cin >> selected_format;

	if((selected_format >= i) || (selected_format < 0))
	{
		cout << "\n\nERROR:  No video format selected\n";
		return E_FAIL;
	}

	CHECK_HR(am_stream_config->GetStreamCaps(selected_format, &video_settings, reinterpret_cast<BYTE*>(&video_stream_config_caps)));
	VIDEOINFOHEADER* video_infoheader = reinterpret_cast<VIDEOINFOHEADER*>(video_settings->pbFormat);
	REFERENCE_TIME avg_time_per_frame = video_infoheader->AvgTimePerFrame;
	video_infoheader->AvgTimePerFrame = static_cast<REFERENCE_TIME>(10000000 / 29);
	
	if(!SUCCEEDED(am_stream_config->SetFormat(video_settings)))
	{
		video_infoheader->AvgTimePerFrame = avg_time_per_frame;
		CHECK_HR(am_stream_config->SetFormat(video_settings));
		CoTaskMemFree((BYTE * ) video_infoheader);
	}
	
	CoTaskMemFree(reinterpret_cast<BYTE *>(video_infoheader));

	if (video_settings->cbFormat != 0)
	{
		video_settings->cbFormat = 0;
		video_settings->pbFormat = NULL;
	}

	CoTaskMemFree(video_settings);

	return S_OK;
}

HRESULT FilterGraph::ConvertGuidToString(const GUID& sub_type, char* four_cc) const 
{
    //This checks the guid to see if it is the standard video 4CC subtype
    if (sub_type.Data2 == 0x0000 && sub_type.Data3 == 0x0010
        && sub_type.Data4[0] == 0x80 && sub_type.Data4[1] == 0x00
        && sub_type.Data4[2] == 0x00 && sub_type.Data4[3] == 0xaa
        && sub_type.Data4[4] == 0x00 && sub_type.Data4[5] == 0x38
        && sub_type.Data4[6] == 0x9b && sub_type.Data4[7] == 0x71)
    {
        // the first 4 characters of the guid are a fourcc number
        // representing the video Subtype
        const unsigned long name = sub_type.Data1;
        four_cc[3] = (name >> 24) & (0xff);
        four_cc[2] = (name >> 16) & (0xff);
        four_cc[1] = (name >> 8) & (0xff);
        four_cc[0] = name & (0xff);
        four_cc[4] = 0;
    }
    else
    {
        //if the video subtype is not a standard 4CC code
	    if (sub_type == MEDIASUBTYPE_RGB8)
		    strcpy_s(four_cc, 5, "RGB8");
	    else if (sub_type == MEDIASUBTYPE_RGB555)
		    strcpy_s(four_cc, 7, "RGB555");
	    else if (sub_type == MEDIASUBTYPE_RGB565)
		    strcpy_s(four_cc, 7, "RGB565");
	    else if (sub_type == MEDIASUBTYPE_RGB24)
		    strcpy_s(four_cc, 6, "RGB24");
	    else if (sub_type == MEDIASUBTYPE_RGB32)
		    strcpy_s(four_cc, 6, "RGB32");
	    else
		    strcpy_s(four_cc, 8, "UNKNOWN");
    }

	return S_OK;
}


HRESULT FilterGraph::FindFilterInCategory(IBaseFilter** src, const IID& cls, std::wstring& filter_name, int device_num)
{
  CComPtr<ICreateDevEnum> device_enums;
  CComPtr<IEnumMoniker> enum_moniker;

  CHECK_HR(CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER, IID_ICreateDevEnum, reinterpret_cast<void**>(&device_enums)));
  CHECK_HR(device_enums->CreateClassEnumerator(cls, &enum_moniker, 0));

  if (enum_moniker == 0)
    return E_FAIL;

  int i = 0;
  for (;;)
  {
    CComPtr<IMoniker> moniker;

    if (enum_moniker->Next(1, &moniker, 0) != S_OK)
	    return E_FAIL;

    if (device_num == i)
    {
	    CComVariant var_name;
	    CComPtr<IPropertyBag> prop_bag;
	    moniker->BindToStorage(NULL, NULL, IID_IPropertyBag, reinterpret_cast<void**>(&prop_bag));
	    prop_bag->Read(L"FriendlyName", &var_name, NULL);
	    moniker->BindToObject(NULL, NULL, IID_IBaseFilter, reinterpret_cast<void**>(src));
	    filter_name = V_BSTR(&var_name);
	    var_name.Clear();
	    return S_OK;
    }
    ++i;
  }
  return E_FAIL;
}


HRESULT FilterGraph::GetAudioCaptureSource(IBaseFilter** source, int* channels, int* sample_rate, int* bps)
{
	
	int selected_device = 0;
	wstring filter_name;

	cout << "  Audio capture devices:" << endl;
  int i = 0;
	for(;;)
	{
		CComPtr<IBaseFilter> temp_source;

		HRESULT hr = FindFilterInCategory(&temp_source, CLSID_AudioInputDeviceCategory, filter_name, i);

    if((!SUCCEEDED(hr)) && (i == 0))
		{
			cout << "ERROR: No Audio Capture device could be found" << endl;
			return E_FAIL;
		}
		else if(!SUCCEEDED(hr))
			break;

		wcout << "    " << setw(3) << i << ") " << filter_name.c_str() << endl;		
    ++i;
	} 

	cout << "  Please Select a device (0-" << i-1 << "):";
	cin >> selected_device;

	if(( selected_device >= i) || ( selected_device < 0))
	{
		cout << "ERROR:  No Audio capture device selected." << endl;
		return E_FAIL;
	}

	CHECK_HR(FindFilterInCategory(source, CLSID_AudioInputDeviceCategory, filter_name, selected_device));
	CHECK_HR(SetAudioCaptureFormat(*source, channels, sample_rate, bps));
 
  return S_OK;
}

HRESULT FilterGraph::SetAudioCaptureFormat(IBaseFilter* audio_source, int* channels, int* sample_rate, int* bps)
{
  CComPtr<IAMStreamConfig> am_stream_config;	
	CHECK_HR(capture_graph_builder2_->FindInterface(NULL, NULL, audio_source, IID_IAMStreamConfig, reinterpret_cast<void**>(&am_stream_config)));	

	int count = 0;
	int size = 0;
	
	int selected_device, list_num=0;
	int listNumArray[1024];

	CHECK_HR(am_stream_config->GetNumberOfCapabilities(&count, &size));

	AM_MEDIA_TYPE* audio_settings = NULL;
	AUDIO_STREAM_CONFIG_CAPS audio_stream_config_caps;

	cout << "  Audio capture formats:" << endl;

	for (int i = 0; i < count; ++i)
	{
		CHECK_HR(am_stream_config->GetStreamCaps(i, &audio_settings, (BYTE*)&audio_stream_config_caps)); 

		WAVEFORMATEX* wav_settings = reinterpret_cast<WAVEFORMATEX*>(audio_settings->pbFormat);

		if (((wav_settings->nSamplesPerSec == 44100) 
		  || (wav_settings->nSamplesPerSec == 22050) 
		  || (wav_settings->nSamplesPerSec == 11025)
		  || (wav_settings->nSamplesPerSec == 5512)) 
		  && (wav_settings->wBitsPerSample == 16))
		{
			cout << "    "  << setw(3) << list_num << ") " << wav_settings->nSamplesPerSec  <<" hz - " << wav_settings->nChannels << " channels" << setfill(' ') << endl;
			listNumArray[list_num]=i;
			list_num++;
		}

		CoTaskMemFree((BYTE * ) audio_settings->pbFormat);
		CoTaskMemFree((BYTE * ) audio_settings);            
	}

	cout << "  Please Select a format (0-" << list_num-1 << "):";
	cin >> selected_device;

	if((selected_device >= list_num) || (selected_device < 0))
	{
		cout << "ERROR:  No video format selected" << endl;
		return E_FAIL;
	}

	//AM_MEDIA_TYPE *audio_settings(0);
	//AUDIO_STREAM_CONFIG_CAPS audio_stream_config_caps;

	CHECK_HR(am_stream_config->GetStreamCaps(listNumArray[selected_device], &audio_settings, (BYTE*)&audio_stream_config_caps)); 

	WAVEFORMATEX* wav_settings = reinterpret_cast<WAVEFORMATEX*>(audio_settings->pbFormat);

	CHECK_HR(am_stream_config->SetFormat(audio_settings));

	CoTaskMemFree((BYTE * ) audio_settings->pbFormat);
	CoTaskMemFree((BYTE * ) audio_settings);  

	CComPtr<IAMBufferNegotiation> audio_cap_buffer;

	CHECK_HR(capture_graph_builder2_->FindInterface(NULL , NULL, audio_source, IID_IAMBufferNegotiation, reinterpret_cast<void**>(&audio_cap_buffer)));

	int latency_in_ms = 100;
	*sample_rate = wav_settings->nSamplesPerSec;
	*channels = wav_settings->nChannels;
	*bps = wav_settings->wBitsPerSample/8;

	ALLOCATOR_PROPERTIES Aprop;               
	Aprop.cbAlign  =- 1;
	Aprop.cbBuffer = ((*sample_rate)*(*bps)*(*channels)*latency_in_ms)/1000;
	Aprop.cbPrefix =- 1;
	Aprop.cBuffers =- 1;
	
  CHECK_HR(audio_cap_buffer->SuggestAllocatorProperties(&Aprop));

  return S_OK;
}

HRESULT FilterGraph::Run()
{
  CHECK_HR(graph_builder_->QueryInterface(IID_IMediaControl, reinterpret_cast<void**>(&media_control_)));
  CHECK_HR(graph_builder_->QueryInterface(IID_IMediaEvent, reinterpret_cast<void**>(&media_event_)));

  CHECK_HR(monitor_running_graph.Start(graph_builder_));

  cout << "  Graph is running. Press Ctrl+C to terminate immediately." << endl;
  CHECK_HR(media_control_->Run());
  
  const HANDLE handle_post = CreateEvent(NULL, FALSE, FALSE, NULL); // auto-reset nonsignaled event
  const HANDLE ha[] = {g_handle_quit, handle_post};
  unsigned int thread_id;
   
  LivePoster::ip_address_ = server_ip_;
  LivePoster::port_ = port_num_;
  LivePoster::webm_file_ = out_webm_file_;
    
  //const HANDLE thread_handle = (HANDLE)_beginthreadex(NULL, 0, LivePoster::ThreadProc, (void*)ha, 0, &thread_id);
  //const BOOL ret = CloseHandle(thread_handle);
  //assert(ret);
    
  __int64 ctr1 = 0, ctr2 = 0, freq = 0;
  QueryPerformanceCounter((LARGE_INTEGER*)&ctr1);
  for (;;)
  {
    const DWORD dw = WaitForSingleObject(g_handle_quit, 10);
    
    if (dw == (WAIT_OBJECT_0)) // Ctrl+C pressed
    {
      cout << endl << "  Terminated by Ctrl + C" << endl;
      CHECK_HR(media_control_->Stop());
      SetEvent(g_handle_quit);
      break;
    }

    QueryPerformanceCounter((LARGE_INTEGER*)&ctr2);    
  	QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
   	double span = (ctr2 - ctr1)* 1.0 /freq;
   	
   	if (span > 0.05) // Every 50 milliseconds.
   	{
      //SetEvent(handle_post);
      ctr1 = ctr2;
      cout << ".";  //shows that the graph is still running
    }
  }

  CHECK_HR(monitor_running_graph.Stop());

  return S_OK;
}

} // end of live_webm_streamer namespace
