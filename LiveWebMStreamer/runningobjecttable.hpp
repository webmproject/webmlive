#pragma once;

#include <atlbase.h>
#include <DShow.h>

namespace live_webm_streamer {

class RunningObjectTable
{
public:
  RunningObjectTable();
  ~RunningObjectTable();
  HRESULT Start(const IGraphBuilder* graph_builder);
  HRESULT Stop();
private:
  CComPtr<IRunningObjectTable> rot;
  CComPtr<IMoniker> moniker;
  DWORD rot_entry;
};

}