// webmtestshell_2008.cpp : Defines the entry point for the console application.
//

#include "http_client_base.h"

#include <conio.h>
#include <stdio.h>
#include <tchar.h>

#include <iostream>
#include <string>


#pragma warning(push)
#pragma warning(disable:4512)
#include "boost/program_options.hpp"
#pragma warning(pop)
#include "boost/thread/thread.hpp"

#include "debug_util.h"
#include "http_uploader.h"

int _tmain(int argc, _TCHAR* argv[])
{
  using std::cerr;
  using std::cout;

  // abbreviate boost::program_options to preserve sanity!
  namespace po = boost::program_options;

  // configure our command line opts; for now we only have one group
  // "Basic options"
  po::options_description opts_desc("Required options");
  opts_desc.add_options()
    ("help", "Show this help message.")
    ("file", po::value<std::string>(), "Path to local WebM file.")
    ("url", po::value<std::string>(), "Destination for HTTP Post.");

  // parse and store the command line options
  po::variables_map var_map;
  po::store(po::parse_command_line(argc, argv, opts_desc), var_map);
  po::notify(var_map);

  // user asked for help
  if (var_map.count("help")) {
    cerr << opts_desc << "\n";
    return EXIT_FAILURE;
  }

  // validate params
  // TODO(tomfinegan): can probably make program options enforce this for me...
  if (!var_map.count("file") || !var_map.count("url")) {
    cerr << "file and url params are required!\n";
    cerr << opts_desc << "\n";
    return EXIT_FAILURE;
  }

  DBGLOG("file: " << var_map["file"].as<std::string>().c_str());
  DBGLOG("url: " << var_map["url"].as<std::string>().c_str());

  HttpUploaderSettings settings;
  settings.local_file = var_map["file"].as<std::string>();
  settings.target_url = var_map["url"].as<std::string>();

  HttpUploader uploader;
  if (uploader.Init(&settings) != 0) {
    cerr << "uploader init failed.\n";
    return EXIT_FAILURE;
  }

  uploader.Go();

  // hooray, we survived arg parsing... let's do something

  cout << "press a key to exit...\n";

  while(!_kbhit()) {
    Sleep(1);
  }

  uploader.Stop();

  return EXIT_SUCCESS;
}


// We build with BOOST_NO_EXCEPTIONS defined; boost will call this function
// instead of throwing.  We must stop execution here.
void boost::throw_exception(const std::exception& e)
{
  using std::cerr;
  cerr << "Fatal error: " << e.what() << "\n";
  exit(EXIT_FAILURE);
}
