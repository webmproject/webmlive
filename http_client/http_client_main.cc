// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "http_client_base.h"

#include <conio.h>
#include <stdio.h>
#include <tchar.h>

#include <cstdio>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#pragma warning(push)
#pragma warning(disable:4512)
#include "boost/program_options.hpp"
#pragma warning(pop)
#include "boost/scoped_array.hpp"
#include "boost/thread/thread.hpp"

#include "debug_util.h"
#include "file_reader.h"
#include "http_uploader.h"
#include "webm_encoder.h"

void set_command_line_options(
    boost::program_options::options_description& opts_desc) {
  namespace po = boost::program_options;
  opts_desc.add_options()
      ("help", "Show this help message.")
      ("file", po::value<std::string>(), "Path for local WebM file.")
      ("url", po::value<std::string>(), "Destination for HTTP Post.")
      // use of |composing| tells program_options to collect multiple --header
      // instances into a single vector of strings
      ("header",
       po::value<std::vector<std::string>>()->composing(),
       "HTTP header, must be specified as name:value.")
      ("var",
       po::value<std::vector<std::string>>()->composing(),
       "Form variable, must be specified as name:value.");
}

int store_string_map_entries(const std::vector<std::string>& unparsed_entries,
                             std::map<std::string, std::string>& out_map)
{
  using std::string;
  using std::vector;
  vector<string>::const_iterator entry_iter = unparsed_entries.begin();
  while (entry_iter != unparsed_entries.end()) {
    // TODO(tomfinegan): support empty headers?
    const string& entry = *entry_iter;
    size_t sep = entry.find(":");
    if (sep == string::npos) {
      // bad header (missing separator, no value)
      // TODO(tomfinegan): allow empty entries?
      DBGLOG("ERROR: cannot parse entry, should be name:value, got="
             << entry.c_str());
      return ERROR_BAD_FORMAT;
    }
    out_map[entry.substr(0, sep).c_str()] = entry.substr(sep+1);
    ++entry_iter;
  }
  return ERROR_SUCCESS;
}

// Calls |Init| and |Run| on |encoder| to start the encode of a WebM file.
// TODO(tomfinegan): Add capture and encoder settings configuration.
int start_encoder(webmlive::WebmEncoder& encoder,
                  const webmlive::HttpUploaderSettings& settings) {
  int status = encoder.Init(settings.local_file);
  if (status) {
    DBGLOG("encoder Init failed, status=" << status);
    return status;
  }
  status = encoder.Run();
  if (status) {
    DBGLOG("encoder Run failed, status=" << status);
  }
  return status;
}

// Calls |Init| and |Run| on |uploader| to start the uploader thread, which
// uploads buffers when |UploadBuffer| is called on the uploader.
int start_uploader(webmlive::HttpUploader& uploader,
                   webmlive::HttpUploaderSettings& settings) {
  int status = uploader.Init(&settings);
  if (status) {
    DBGLOG("uploader Init failed, status=" << status);
    return status;
  }
  status = uploader.Run();
  if (status) {
    DBGLOG("uploader Run failed, status=" << status);
  }
  return status;
}

int client_main(webmlive::HttpUploaderSettings& settings) {
  // Setup the file reader.  This is a little strange since |reader| actually
  // creates the output file that is used by the encoder.
  webmlive::FileReader reader;
  int status = reader.CreateFile(settings.local_file);
  if (status) {
    fprintf(stderr, "file reader init failed, status=%d.\n", status);
    return EXIT_FAILURE;
  }
  // Start encoding the WebM file.
  webmlive::WebmEncoder encoder;
  status = start_encoder(encoder, settings);
  if (status) {
    fprintf(stderr, "start_encoder failed, status=%d\n", status);
    return EXIT_FAILURE;
  }
  // Start the uploader thread.
  webmlive::HttpUploader uploader;
  status = start_uploader(uploader, settings);
  if (status) {
    fprintf(stderr, "start_uploader failed, status=%d\n", status);
    encoder.Stop();
    return EXIT_FAILURE;
  }
  webmlive::HttpUploaderStats stats;
  const size_t kReadBufferSize = 100*1024;
  using boost::scoped_array;
  scoped_array<uint8> read_buf(new (std::nothrow) uint8[kReadBufferSize]);
  if (!read_buf) {
    uploader.Stop();
    encoder.Stop();
    fprintf(stderr, "out of memory, can't alloc read_buf.\n");
    return EXIT_FAILURE;
  }
  // Loop until the user hits a key.
  printf("\nPress the any key to quit...\n");
  while(!_kbhit()) {
    // Output current duration and upload progress
    if (uploader.GetStats(&stats) == webmlive::HttpUploader::kSuccess) {
      printf("\rencoded duration: %04f seconds, uploaded: %I64d @ %d kBps",
             encoder.encoded_duration(), stats.bytes_sent,
             static_cast<int>(stats.bytes_per_second / 1000));
    }
    // Check if the upload thread is ready
    if (uploader.UploadComplete()) {
      // Read some data
      size_t bytes_read = 0;
      status = reader.Read(kReadBufferSize, &read_buf[0], &bytes_read);
      if (status && status != webmlive::FileReader::kAtEOF) {
        DBGLOG("Read failed, status=" << status);
        //fprintf(stderr, "\nERROR: can't read from file!\n");
        //break;
        Sleep(100);
      }
      if (bytes_read > 0) {
        // Start upload of the read buffer contents
        DBGLOG("starting buffer upload, bytes_read=" << bytes_read);
        status = uploader.UploadBuffer(&read_buf[0], bytes_read);
        if (status) {
          DBGLOG("UploadBuffer failed, status=" << status);
          fprintf(stderr, "\nERROR: can't upload buffer!\n");
          break;
        }
      }
    }
    Sleep(100);
  }
  DBGLOG("stopping encoder...");
  encoder.Stop();
  DBGLOG("stopping uploader...");
  uploader.Stop();
  printf("\nDone.\n");
  return EXIT_SUCCESS;
}

int _tmain(int argc, _TCHAR* argv[]) {
  namespace po = boost::program_options;
  // configure our command line opts; for now we only have one group
  // "Basic options"
  po::options_description opts_desc("Required options");
  set_command_line_options(opts_desc);
  // parse and store the command line options
  po::variables_map var_map;
  po::store(po::parse_command_line(argc, argv, opts_desc), var_map);
  po::notify(var_map);
  // user asked for help
  if (var_map.count("help")) {
    std::ostringstream usage_string;
    usage_string << opts_desc << "\n";
    fprintf(stderr, "%s", usage_string.str().c_str());
    return EXIT_FAILURE;
  }
  // validate params
  // TODO(tomfinegan): can probably make program options enforce this for me...
  if (!var_map.count("file") || !var_map.count("url")) {
    fprintf(stderr, "file and url params are required!\n");
    std::ostringstream usage_string;
    usage_string << opts_desc << "\n";
    fprintf(stderr, "%ls", usage_string.str().c_str());
    return EXIT_FAILURE;
  }
  DBGLOG("file: " << var_map["file"].as<std::string>().c_str());
  DBGLOG("url: " << var_map["url"].as<std::string>().c_str());
  // TODO(tomfinegan): Need to add capture and encoder settings...
  webmlive::HttpUploaderSettings settings;
  settings.local_file = var_map["file"].as<std::string>();
  settings.target_url = var_map["url"].as<std::string>();
  // Parse and store any HTTP header name:value pairs passed via command line.
  if (var_map.count("header")) {
    using std::string;
    using std::vector;
    const vector<string>& headers = var_map["header"].as<vector<string>>();
    if (store_string_map_entries(headers, settings.headers)) {
      fprintf(stderr, "ERROR: command line HTTP header parse failed!");
      return EXIT_FAILURE;
    }
  }
  // Parse and store any form variable name:value pairs passed via command
  // line.
  if (var_map.count("var")) {
    using std::string;
    using std::vector;
    const vector<string>& form_vars = var_map["var"].as<vector<string>>();
    if (store_string_map_entries(form_vars, settings.form_variables)) {
      fprintf(stderr, "ERROR: command line form variable parse failed!");
      return EXIT_FAILURE;
    }
  }
  return client_main(settings);
}


// We build with BOOST_NO_EXCEPTIONS defined; boost will call this function
// instead of throwing.  We must stop execution here.
void boost::throw_exception(const std::exception& e) {
  fprintf(stderr, "Fatal error: %s\n", e.what());
  exit(EXIT_FAILURE);
}
