#pragma once

#include <fstream>
#include <curl\curl.h>

using namespace std;

namespace live_webm_streamer {

class LivePoster
{
public:
  LivePoster();
  ~LivePoster(void);

  static wchar_t* webm_file_;
  static wchar_t* ip_address_;
  static wchar_t* port_;
  
  static unsigned __stdcall ThreadProc(void*);
  
private:
  static long size_file_;   
  static bool init_;
  static bool codec_private_checked_;
};

}
