#pragma once

namespace live_webm_streamer 
{
class IHttpPost
{
public:
  virtual void* CreateEvent_() = 0;
  virtual int SetEvent_() = 0;
  virtual void* CreateThread_() = 0;
  virtual ~IHttpPost(void);
};
}