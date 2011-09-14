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
#include "glog/logging.h"
#include "http_uploader.h"
#include "webm_encoder.h"

#undef DBGLOG
#define DBGLOG(X) LOG(ERROR) << X

namespace {
enum {
  kBadFormat = -3,
  kNoMemory = -2,
  kInvalidArg = - 1,
  kSuccess = 0,
};

const std::string kMetadataQueryFragment = "&metadata=1";
const std::string kWebmItagQueryFragment = "&itag=43";
typedef std::vector<std::string> StringVector;

struct WebmEncoderClientConfig {
  // Target for HTTP POSTs.
  std::string target_url;
  // Uploader settings.
  webmlive::HttpUploaderSettings uploader_settings;
  // WebM encoder settings.
  webmlive::WebmEncoderConfig enc_config;
};

}  // anonymous namespace

// Prints usage.
void usage(const char** argv) {
  printf("Usage: %ls --file <output file name> --url <target URL>\n", argv[0]);
  printf("  Notes: \n");
  printf("    The file and url params are always required.\n");
  printf("    The stream_id and stream_name params are required when the\n");
  printf("    url lacks a query string.\n");
  printf("  General Options:\n");
  printf("    -h | -? | --help               Show this message and exit.\n");
  printf("    --adev <audio source name>     Audio capture device name.\n");
  printf("    --file <output file name>      Path to output WebM file.\n");
  printf("    --form_post                    Send WebM chunks as file data\n");
  printf("                                   in an form (a la RFC 1867).\n");
  printf("    --stream_id <stream ID>        Stream ID to include in POST\n");
  printf("                                   query string.\n" );
  printf("    --stream_name <stream name>    Stream name to include in POST\n");
  printf("                                   query string.\n" );
  printf("    --url <target URL>             Target for HTTP Posts.\n");
  printf("    --vdev <video source name>     Video capture device name.\n");
  printf("  Vorbis Encoder options:\n");
  printf("    --vorbis_bitrate <kbps>            Audio bitrate.\n");
  printf("  VPX Encoder options:\n");
  printf("    --vpx_bitrate <kbps>               Video bitrate.\n");
  printf("    --vpx_keyframe_interval <seconds>  Time between keyframes.\n");
  printf("    --vpx_min_q <min q value>          Quantizer minimum.\n");
  printf("    --vpx_max_q <max q value>          Quantizer maximum.\n");
  printf("    --vpx_speed <speed value>          Speed.\n");
  printf("    --vpx_static_threshold <threshold> Static threshold.\n");
  printf("    --vpx_speed <speed value>          Speed.\n");
  printf("    --vpx_threads <num threads>        Number of encode threads.\n");
  printf("    --vpx_token_partitions <0-3>       Number of token\n");
  printf("                                       partitions.\n");
  printf("    --vpx_undershoot <undershoot>      Undershoot value.\n");
}

// Parses name value pairs in the format name:value from |unparsed_entries|,
// and stores results in |out_map|.
int store_string_map_entries(const StringVector& unparsed_entries,
                             std::map<std::string, std::string>& out_map)
{
  using std::string;
  using std::vector;
  StringVector::const_iterator entry_iter = unparsed_entries.begin();
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

// Parses command line and stores user settings.
void parse_command_line(int argc, const char** argv,
                        WebmEncoderClientConfig& config) {
  StringVector unparsed_headers;
  StringVector unparsed_vars;
  webmlive::HttpUploaderSettings& uploader_settings = config.uploader_settings;
  webmlive::WebmEncoderConfig& enc_config = config.enc_config;
  config.uploader_settings.post_mode = webmlive::HTTP_POST;
  for (int i = 1; i < argc; ++i) {
    if (!strcmp("-h", argv[i]) || !strcmp("-?", argv[i]) ||
        !strcmp("--help", argv[i])) {
      usage(argv);
      exit(EXIT_SUCCESS);
    } else if (!strcmp("--file", argv[i])) {
      uploader_settings.local_file = argv[++i];
      enc_config.output_file_name = uploader_settings.local_file;
    } else if (!strcmp("--url", argv[i])) {
      config.target_url = argv[++i];
    } else if (!strcmp("--header", argv[i])) {
      unparsed_headers.push_back(argv[++i]);
    } else if (!strcmp("--var", argv[i])) {
      unparsed_vars.push_back(argv[++i]);
    } else if (!strcmp("--adev", argv[i])) {
      enc_config.audio_device_name = argv[++i];
    } else if (!strcmp("--vdev", argv[i])) {
      enc_config.video_device_name = argv[++i];
    } else if (!strcmp("--stream_name", argv[i])) {
      uploader_settings.stream_name = argv[++i];
    } else if (!strcmp("--stream_id", argv[i])) {
      uploader_settings.stream_id = argv[++i];
    } else if (!strcmp("--form_post", argv[i])) {
      uploader_settings.post_mode = webmlive::HTTP_FORM_POST;
    } else if (!strcmp("--vorbis_bitrate", argv[i])) {
      char* ptr_end;
      enc_config.vorbis_bitrate = strtol(argv[++i], &ptr_end, 10);
    } else if (!strcmp("--vpx_keyframe_interval", argv[i])) {
      char* ptr_end;
      enc_config.vpx_config.keyframe_interval = strtod(argv[++i], &ptr_end);
    } else if (!strcmp("--vpx_bitrate", argv[i])) {
      char* ptr_end;
      enc_config.vpx_config.bitrate = strtol(argv[++i], &ptr_end, 10);
    } else if (!strcmp("--vpx_min_q", argv[i])) {
      char* ptr_end;
      enc_config.vpx_config.min_quantizer = strtol(argv[++i], &ptr_end, 10);
    } else if (!strcmp("--vpx_max_q", argv[i])) {
      char* ptr_end;
      enc_config.vpx_config.max_quantizer = strtol(argv[++i], &ptr_end, 10);
    } else if (!strcmp("--vpx_speed", argv[i])) {
      char* ptr_end;
      enc_config.vpx_config.speed = strtol(argv[++i], &ptr_end, 10);
    } else if (!strcmp("--vpx_static_threshold", argv[i])) {
      char* ptr_end;
      enc_config.vpx_config.static_threshold = strtol(argv[++i], &ptr_end, 10);
    } else if (!strcmp("--vpx_threads", argv[i])) {
      char* ptr_end;
      enc_config.vpx_config.thread_count = strtol(argv[++i], &ptr_end, 10);
    } else if (!strcmp("--vpx_token_partitions", argv[i])) {
      char* ptr_end;
      enc_config.vpx_config.token_partitions = strtol(argv[++i], &ptr_end, 10);
    } else if (!strcmp("--vpx_undershoot", argv[i])) {
      char* ptr_end;
      enc_config.vpx_config.undershoot = strtol(argv[++i], &ptr_end, 10);
    }
  }
  // Store user HTTP headers.
  store_string_map_entries(unparsed_headers, uploader_settings.headers);
  // Store user form variables.
  store_string_map_entries(unparsed_vars, uploader_settings.form_variables);
}

// Calls |Init| and |Run| on |encoder| to start the encode of a WebM file.
int start_encoder(webmlive::WebmEncoder& encoder,
                  const webmlive::WebmEncoderConfig& settings) {
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
                   WebmEncoderClientConfig& config) {
  if (config.target_url.find('?') == std::string::npos) {
    // No query string-- reconstruct the URL.
    std::ostringstream url;
    // rebuild the url with query params included
    url << config.target_url << "?ns=" << config.uploader_settings.stream_name
        << "&id=" << config.uploader_settings.stream_id
        << kWebmItagQueryFragment
        << kMetadataQueryFragment;
    config.target_url = url.str();
  } else {
    // The user specified a query string; assume that the user knows what
    // they're doing, and append |kMetadataQueryFragment|.
    config.target_url.append(kMetadataQueryFragment);
  }

  int status = uploader.Init(config.uploader_settings);
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

int client_main(WebmEncoderClientConfig& config) {
  // Setup the file reader.  This is a little strange since |reader| actually
  // creates the output file that is used by the encoder.
  webmlive::HttpUploaderSettings& uploader_settings = config.uploader_settings;
  webmlive::FileReader reader;
  int status = reader.CreateFile(uploader_settings.local_file);
  if (status) {
    fprintf(stderr, "file reader init failed, status=%d.\n", status);
    return EXIT_FAILURE;
  }
  // Start encoding the WebM file.
  webmlive::WebmEncoderConfig& enc_config = config.enc_config;
  webmlive::WebmEncoder encoder;
  status = start_encoder(encoder, enc_config);
  if (status) {
    fprintf(stderr, "start_encoder failed, status=%d\n", status);
    return EXIT_FAILURE;
  }
  // Start the uploader thread.
  webmlive::HttpUploader uploader;
  status = start_uploader(uploader, config);
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
  int num_posts = 0;
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
        if (num_posts == 1) {
          // Remove the metadata query fragment from the URL after the first
          // post.  It is only present for the first post, which includes
          // the EBML header, MKV segment Info, and MKV segment tracks
          // elements.
          std::string& url = config.target_url;
          url.erase(url.find(kMetadataQueryFragment),
                    kMetadataQueryFragment.length());
        }
        // Start upload of the read buffer contents
        DBGLOG("starting buffer upload, chunk_length=" << chunk_length);
        status = uploader.UploadBuffer(&read_buf[0], chunk_length,
                                       config.target_url);
        if (status) {
          DBGLOG("UploadBuffer failed, status=" << status);
          fprintf(stderr, "\nERROR: can't upload buffer!\n");
          exit_code = EXIT_FAILURE;
          break;
        }
        ++num_posts;
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

int main(int argc, const char** argv) {
  google::InitGoogleLogging(argv[0]);
  WebmEncoderClientConfig config;
  config.enc_config = webmlive::WebmEncoder::DefaultConfig();
  parse_command_line(argc, argv, config);
  // validate params
  if (config.target_url.empty() ||
      config.enc_config.output_file_name.empty()) {
    fprintf(stderr, "file and url params are required!\n");
    usage(argv);
    return EXIT_FAILURE;
  }
  if ((config.uploader_settings.stream_id.empty() ||
      config.uploader_settings.stream_name.empty()) &&
      config.target_url.find('?') == std::string::npos) {
    fprintf(stderr, "stream_id and stream_name are required when the target "
            "url lacks a query string!\n");
    return EXIT_FAILURE;
  }
  DBGLOG("file: " << config.enc_config.output_file_name.c_str());
  DBGLOG("url: " << config.target_url.c_str());
  return client_main(config);
}

// We build with BOOST_NO_EXCEPTIONS defined; boost will call this function
// instead of throwing.  We must stop execution here.
void boost::throw_exception(const std::exception& e) {
  fprintf(stderr, "Fatal error: %s\n", e.what());
  exit(EXIT_FAILURE);
}
