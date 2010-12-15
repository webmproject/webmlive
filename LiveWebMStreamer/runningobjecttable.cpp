#include "runningobjecttable.hpp"
#include "filtergraph.hpp"

namespace live_webm_streamer {

RunningObjectTable::RunningObjectTable():
    rot(NULL),
    moniker(NULL),
    rot_entry(0)
{
}

RunningObjectTable::~RunningObjectTable()
{
}

HRESULT RunningObjectTable::Start(const IGraphBuilder* graph_builder)
{
  CHECK_HR(GetRunningObjectTable(0, &rot));

  std::wostringstream os;
  os << "FilterGraph " << std::hex << &graph_builder << " pid " << GetCurrentProcessId();

  CHECK_HR(CreateItemMoniker(L"!", os.str().c_str(), &moniker));
  CHECK_HR(rot->Register(ROTFLAGS_REGISTRATIONKEEPSALIVE, const_cast<IGraphBuilder*>(graph_builder), moniker, &rot_entry));

  return S_OK;
}

HRESULT RunningObjectTable::Stop()
{
  CHECK_HR(rot->Revoke(rot_entry));
  return S_OK;
}

} // end of the namespace