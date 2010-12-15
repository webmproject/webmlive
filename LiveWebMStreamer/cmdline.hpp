#pragma once

#include <iostream>

using namespace std;

namespace live_webm_streamer {

class Uncopyable
{
protected:
  Uncopyable(){};
  ~Uncopyable(){};  
private:
  Uncopyable(const Uncopyable&);
  Uncopyable& operator=(const Uncopyable&);
};

class CmdLine/* : private Uncopyable*/
{
public:
  CmdLine(void);
  ~CmdLine(void);
  
  bool ParseArg(int argc, char* argv[]);
  void PrintUsage() const;
  char* GetServerIp() const;
  char* GetPortNumber() const;
  char* GetFileName() const;
  
private:
  char* server_ip_;
  char* port_num_;  
  char* out_webm_file_;
  
  bool HandleCommon(int argc, char* argv[]);
  
};

}