#include "filtergraph.hpp"
#include "livewebmstreamer.hpp"

using namespace std;
using namespace live_webm_streamer;

HANDLE g_handle_quit;

static bool _stdcall ConsoleCtrlHandler(DWORD type)
{
  if (type == CTRL_C_EVENT)
  {
    const BOOL b = SetEvent(g_handle_quit);
    assert(b);
    return true;
  }
  else
    return false;
}

int wmain(int argc, wchar_t* argv[])
{
  g_handle_quit = CreateEvent(NULL, FALSE, FALSE, NULL);
  assert(g_handle_quit);
  const BOOL b = SetConsoleCtrlHandler((PHANDLER_ROUTINE)ConsoleCtrlHandler, TRUE);
  assert(b);

  HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
  assert(SUCCEEDED(hr));
  {
    FilterGraph filter_graph;
    if (!filter_graph.Parse(argc, argv))
    {
      hr = filter_graph.SetUp();
      assert(SUCCEEDED(hr));
      
      hr = filter_graph.Run();
      assert(SUCCEEDED(hr));
    }
  }
  const BOOL b1 = CloseHandle(g_handle_quit);
  assert(b1);

  CoUninitialize();
}
