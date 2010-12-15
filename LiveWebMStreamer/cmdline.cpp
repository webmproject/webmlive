#include <string.h>
#include <cassert>

#include "cmdline.hpp"



namespace live_webm_streamer {

CmdLine::CmdLine(void)
{
}

CmdLine::~CmdLine(void)
{
  delete [] server_ip_;
  delete [] port_num_;
  delete [] out_webm_file_;
}
 
bool CmdLine::ParseArg(int argc, char* argv[])
{
  if (argc <= 2)
  {
    PrintUsage();
    return true;  
  }
  
  HandleCommon(argc, argv);
 
  return false;
}

bool CmdLine::HandleCommon(int argc, char* argv[])
{
  const size_t len_ip = strlen(argv[1]);
  assert(len_ip != 0);
  
  server_ip_ = new char[len_ip+1];
  strcpy_s(server_ip_, len_ip+1, argv[1]);
  //TODO: handle server_ip with more verification

  const size_t len_port = strlen(argv[2]);
  assert(len_port != 0);
  
  port_num_ = new char[len_port+1];
  strcpy_s(port_num_, len_port+1, argv[2]);
  //TODO: handle port with more verification
  
  if (argc == 3)
  {
    const char default_webm_file[] = "test.webm";
    
    const size_t len_webm_file = strlen(default_webm_file);
    out_webm_file_ = new char[len_webm_file+1];
    strcpy_s(out_webm_file_, len_webm_file+1, default_webm_file);
  }
  else if (argc == 4)
  {
    const size_t len_webm_file = strlen(argv[3]);
    assert(len_webm_file != 0);
    
    out_webm_file_ = new char[len_webm_file+1];
    strcpy_s(out_webm_file_, len_webm_file+1, argv[3]);
  }
  else
  {
    // TODO(Hwasoo) : Handle here gracefully.
     ;
  }
    
  return false;
}

void CmdLine::PrintUsage() const
{
  wcout << L"usage: livewebmstreamer <server_ip> <port_number> <webm file>\n";
  wcout << L"example: livewebmstremer 127.0.0.1 8080 test.webm\n";
  return;
}

char* CmdLine::GetPortNumber() const
{
  return port_num_;
}

char* CmdLine::GetServerIp() const
{
  return server_ip_;
}

char* CmdLine::GetFileName() const
{
  return out_webm_file_;
}
}