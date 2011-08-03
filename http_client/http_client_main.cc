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

#include <string>
#include <vector>

#include "boost/scoped_array.hpp"

#include "debug_util.h"
#include "buffer_util.h"
#include "file_reader.h"
#include "http_uploader.h"
#include "webm_encoder.h"

namespace {
enum {
  kBadFormat = -3,
  kNoMemory = -2,
  kInvalidArg = - 1,
  kSuccess = 0,
};
const double kDefaultKeyframeInterval = 2.0;
typedef std::vector<std::string> StringVector;
typedef std::vector<std::wstring> WStringVector;
}  // anonymous namespace

// Prints usage.
void usage(const wchar_t** argv) {
  printf("Usage: %ls --file <output file name> --url <target URL>\n", argv[0]);
  printf("  Note: file and url params are required.\n");
  printf("Options:\n");
  printf("  -h | -? | --help               Show this message and exit.\n");
  printf("  --file <output file name>      Path to output WebM file.\n");
  printf("  --keyframe_interval <seconds>  Time between keyframes.\n");
  printf("  --url <target URL>             Target for HTTP Posts.\n");
}

// Parses name value pairs in the format name:value from |unparsed_entries|,
// and stores results in |out_map|.
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
      DBGLOG("ERROR: cannot parse entry, should be name:value, got="
             << entry.c_str());
      return kBadFormat;
    }
    out_map[entry.substr(0, sep).c_str()] = entry.substr(sep+1);
    ++entry_iter;
  }
  return kSuccess;
}

// Converts |wstr| to a multi-byte string and stores the result in |str|.
int convert_wstring_to_string(const std::wstring& wstr, std::string& str) {
  // Conversion buffer for |wcstombs| calls.
  const size_t buf_size = wstr.length() + 1;
  boost::scoped_array<char> temp_str(new (std::nothrow) char[buf_size]);
  if (!temp_str) {
    fprintf(stderr, "Cannot allocate wide char conversion buffer!\n");
    return kNoMemory;
  }
  memset(temp_str.get(), 0, buf_size);
  wcstombs(temp_str.get(), wstr.c_str(), wstr.length());
  str = temp_str.get();
  return kSuccess;
}

// Converts |std::wstring| entries in |wstrings| to multi-byte strings via
// |wcstombs| and stores them in |strings|.
int convert_wstring_vector_to_string_vector(const WStringVector& wstrings,
                                            StringVector& strings) {
  // Convert |wstrings| vals and store them in |strings|.
  WStringVector::const_iterator i = wstrings.begin();
  std::string temp_str;
  for (; i != wstrings.end(); ++i) {
    int status = convert_wstring_to_string(*i, temp_str);
    if (status) {
      DBGLOG("conversion failed, status=" << status);
      return status;
    }
    strings.push_back(std::string(temp_str));
  }
  return kSuccess;
}

// Parses command line and stores user settings.
void parse_command_line(int argc, const wchar_t** argv,
                        webmlive::HttpUploaderSettings& uploader_settings,
                        webmlive::WebmEncoderSettings& encoder_settings) {
  WStringVector unparsed_wchar_headers;
  WStringVector unparsed_wchar_vars;
  encoder_settings.keyframe_interval = kDefaultKeyframeInterval;
  for (int i = 1; i < argc; ++i) {
    if (!wcscmp(L"-h", argv[i]) || !wcscmp(L"-?", argv[i]) ||
        !wcscmp(L"--help", argv[i])) {
      usage(argv);
      exit(EXIT_SUCCESS);
    } else if (!wcscmp(L"--file", argv[i])) {
      int status = convert_wstring_to_string(std::wstring(argv[++i]),
                                             uploader_settings.local_file);
      if (status) {
        fprintf(stderr, "file name wchar->char conversion failed, status=%d\n",
                status);
        exit(EXIT_FAILURE);
      }
      encoder_settings.output_file_name = uploader_settings.local_file;
    } else if (!wcscmp(L"--url", argv[i])) {
      int status = convert_wstring_to_string(std::wstring(argv[++i]),
                                             uploader_settings.target_url);
      if (status) {
        fprintf(stderr, "URL wchar->char conversion failed, status=%d\n",
                status);
        exit(EXIT_FAILURE);
      }
    } else if (!wcscmp(L"--header", argv[i])) {
      unparsed_wchar_headers.push_back(argv[++i]);
    } else if (!wcscmp(L"--var", argv[i])) {
      unparsed_wchar_vars.push_back(argv[++i]);
    } else if (!wcscmp(L"--keyframe_interval", argv[i])) {
      wchar_t* ptr_end;
      encoder_settings.keyframe_interval = wcstod(argv[++i], &ptr_end);
    }
  }
  // Store user HTTP headers.
  StringVector unparsed_headers;
  int error = convert_wstring_vector_to_string_vector(unparsed_wchar_headers,
                                                      unparsed_headers);
  if (error) {
    fprintf(stderr, "Cannot convert wchar headers to char!\n");
    exit(EXIT_FAILURE);
  }
  store_string_map_entries(unparsed_headers, uploader_settings.headers);
  // Store user form variables.
  StringVector unparsed_vars;
  error = convert_wstring_vector_to_string_vector(unparsed_wchar_vars,
                                                  unparsed_vars);
  if (error) {
    fprintf(stderr, "Cannot convert wchar form variables to char!\n");
    exit(EXIT_FAILURE);
  }
  store_string_map_entries(unparsed_vars, uploader_settings.form_variables);
}


// Calls |Init| and |Run| on |encoder| to start the encode of a WebM file.
int start_encoder(webmlive::WebmEncoder& encoder,
                  const webmlive::WebmEncoderSettings& settings) {
  int status = encoder.Init(settings);
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
  int status = uploader.Init(settings);
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

int client_main(webmlive::HttpUploaderSettings& uploader_settings,
                const webmlive::WebmEncoderSettings& encoder_settings) {
  // Setup the file reader.  This is a little strange since |reader| actually
  // creates the output file that is used by the encoder.
  webmlive::FileReader reader;
  int status = reader.CreateFile(uploader_settings.local_file);
  if (status) {
    fprintf(stderr, "file reader init failed, status=%d.\n", status);
    return EXIT_FAILURE;
  }
  // Start encoding the WebM file.
  webmlive::WebmEncoder encoder;
  status = start_encoder(encoder, encoder_settings);
  if (status) {
    fprintf(stderr, "start_encoder failed, status=%d\n", status);
    return EXIT_FAILURE;
  }
  // Start the uploader thread.
  webmlive::HttpUploader uploader;
  status = start_uploader(uploader, uploader_settings);
  if (status) {
    fprintf(stderr, "start_uploader failed, status=%d\n", status);
    encoder.Stop();
    return EXIT_FAILURE;
  }
  const int32 kReadBufferSize = 100*1024;
  int32 read_buffer_size = kReadBufferSize;
  using boost::scoped_array;
  scoped_array<uint8> read_buf(new (std::nothrow) uint8[kReadBufferSize]);
  if (!read_buf) {
    uploader.Stop();
    encoder.Stop();
    fprintf(stderr, "out of memory, can't alloc read_buf.\n");
    return EXIT_FAILURE;
  }
  webmlive::WebmChunkBuffer chunk_buffer;
  status = chunk_buffer.Init();
  if (status) {
    uploader.Stop();
    encoder.Stop();
    fprintf(stderr, "can't create chunk buffer.\n");
    return EXIT_FAILURE;
  }
  // Loop until the user hits a key.
  int exit_code = EXIT_SUCCESS;
  webmlive::HttpUploaderStats stats;
  printf("\nPress the any key to quit...\n");
  while(!_kbhit()) {
    // Output current duration and upload progress
    if (uploader.GetStats(&stats) == webmlive::HttpUploader::kSuccess) {
      printf("\rencoded duration: %04f seconds, uploaded: %I64d @ %d kBps",
             encoder.encoded_duration(),
             stats.bytes_sent_current + stats.total_bytes_uploaded,
             static_cast<int>(stats.bytes_per_second / 1000));
    }
    size_t bytes_read = 0;
    status = reader.Read(read_buffer_size, &read_buf[0], &bytes_read);
    if (bytes_read > 0) {
      status = chunk_buffer.BufferData(&read_buf[0],
                                       static_cast<int32>(bytes_read));
      if (status) {
        DBGLOG("BufferData failed, status=" << status);
        fprintf(stderr, "\nERROR: cannot add to chunk buffer!\n");
        exit_code = EXIT_FAILURE;
        break;
      }
    }
    if (uploader.UploadComplete()) {
      int32 chunk_length = 0;
      if (chunk_buffer.ChunkReady(&chunk_length)) {
        if (chunk_length > read_buffer_size) {
          // Reallocate the read buffer-- the chunk is too large.
          read_buf.reset(new (std::nothrow) uint8[chunk_length]);
          if (!read_buf) {
            DBGLOG("read buffer reallocation failed");
            fprintf(stderr, "\nERROR: cannot reallocate read buffer!\n");
            exit_code = EXIT_FAILURE;
            break;
          }
          read_buffer_size = chunk_length;
        }
        status = chunk_buffer.ReadChunk(&read_buf[0], chunk_length);
        if (status) {
          DBGLOG("ReadChunk failed, status=" << status);
          fprintf(stderr, "\nERROR: cannot read chunk!\n");
          exit_code = EXIT_FAILURE;
          break;
        }
        // Start upload of the read buffer contents
        DBGLOG("starting buffer upload, chunk_length=" << chunk_length);
        status = uploader.UploadBuffer(&read_buf[0], chunk_length);
        if (status) {
          DBGLOG("UploadBuffer failed, status=" << status);
          fprintf(stderr, "\nERROR: can't upload buffer!\n");
          exit_code = EXIT_FAILURE;
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
  return exit_code;
}

int _tmain(int argc, const wchar_t** argv) {
  webmlive::HttpUploaderSettings uploader_settings;
  webmlive::WebmEncoderSettings encoder_settings;
  parse_command_line(argc, argv, uploader_settings, encoder_settings);
  // validate params
  if (uploader_settings.target_url.empty() ||
      encoder_settings.output_file_name.empty()) {
    fprintf(stderr, "file and url params are required!\n");
    usage(argv);
    return EXIT_FAILURE;
  }
  DBGLOG("file: " << encoder_settings.output_file_name.c_str());
  DBGLOG("url: " << uploader_settings.target_url.c_str());
  return client_main(uploader_settings, encoder_settings);
}

// We build with BOOST_NO_EXCEPTIONS defined; boost will call this function
// instead of throwing.  We must stop execution here.
void boost::throw_exception(const std::exception& e) {
  fprintf(stderr, "Fatal error: %s\n", e.what());
  exit(EXIT_FAILURE);
}
