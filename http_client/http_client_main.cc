// webmtestshell_2008.cpp : Defines the entry point for the console application.
//

#include "http_client_main.h"

#include <conio.h>
#include <stdio.h>
#include <tchar.h>

#include <iostream>
#include <string>

#pragma warning(push)
#pragma warning(disable:4512)
#include "boost/algorithm/string.hpp"
#include "boost/program_options.hpp"
#pragma warning(pop)

int _tmain(int argc, _TCHAR* argv[])
{
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
    cout << opts_desc << "\n";
    return EXIT_FAILURE;
  }

  // validate params
  // TODO(tomfinegan): can probably make program options enforce this for me...
  if (!var_map.count("file") || !var_map.count("url")) {
    cout << "file and url params are required!\n";
    cout << opts_desc << "\n";
    return EXIT_FAILURE;
  } else {
    cout << "file: " << var_map["file"].as<std::string>() << "\n";
    cout << "url: " << var_map["url"].as<std::string>() << "\n";
  }

  // hooray, we survived arg parsing... let's do something

  printf("press a key to exit...\n");

  while(!_kbhit()) {
    Sleep(1);
  }

  return EXIT_SUCCESS;
}
