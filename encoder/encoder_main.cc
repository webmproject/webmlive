// Copyright (c) 2012 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "encoder/encoder_base.h"

#include <conio.h>
#include <stdio.h>
#include <tchar.h>

#include <memory>
#include <string>
#include <vector>

#include "encoder/buffer_util.h"
#include "encoder/file_writer.h"
#include "encoder/http_uploader.h"
#include "encoder/time_util.h"
#include "encoder/webm_encoder.h"
#include "glog/logging.h"

namespace {
enum {
  kBadFormat = -3,
  kNoMemory = -2,
  kInvalidArg = -1,
  kSuccess = 0,
};

const std::string kCodecVp8 = "vp8";
const std::string kCodecVp9 = "vp9";
typedef std::vector<std::string> StringVector;

struct WebmEncoderConfig {
  WebmEncoderConfig() : enable_file_output(true), enable_http_upload(true) {}
  // Uploader settings.
  webmlive::HttpUploaderSettings uploader_settings;

  // WebM encoder settings.
  webmlive::WebmEncoderConfig enc_config;

  bool enable_file_output;
  bool enable_http_upload;
};

}  // anonymous namespace

// Prints usage.
void Usage(const char** argv) {
  printf("%s v%s\n", webmlive::kEncoderName, webmlive::kEncoderVersion);
  printf("Usage: %s <args>\n", argv[0]);
  printf("  General options:\n");
  printf("    -h | -? | --help               Show this message and exit.\n");
  printf("    --disable_file_output          Disables local file output.\n");
  printf("    --disable_http_upload          Disables upload of output to\n");
  printf("                                   HTTP servers.\n");
  printf("    --adev <audio source name>     Audio capture device name.\n");
  printf("    --adevidx <source index>       Select audio capture device by\n");
  printf("                                   index. Ignored when --adev is\n");
  printf("                                   used.\n");
  printf("    --vdev <video source name>     Video capture device name.\n");
  printf("    --vdevidx <source index>       Select video capture device by\n");
  printf("                                   index. Ignored when --vdev is\n");
  printf("                                   used.\n");
  printf("  DASH encoding options:\n");
  printf("    When the --dash argument is present an MPD file is produced\n");
  printf("    that allows the WebM output to be consumed by DASH WebM\n");
  printf("    players.\n");
  printf("    DASH encoding output is unmuxed; audio and video are output\n");
  printf("    in separate container streams.\n");
  printf("    Default DASH name is webmlive. Default DASH dir is the\n");
  printf("    current working directory.\n");
  printf("    --dash                         Enables DASH output.\n");
  printf("    --dash_dir <dir>               Output directory. Directory\n");
  printf("                                   must exist.\n");
  printf("    --dash_name <name>             MPD file name and DASH chunk\n");
  printf("                                   file name prefix.\n");
  printf("    --dash_start_number <string>   Use string specified instead \n");
  printf("                                   of the value 1 for the\n");
  printf("                                   SegmentTemplate startNumber.\n");
  printf("  HTTP uploader options:\n");
  printf("    Sends WebM chunks to an HTTP server via HTTP POST. Enabled\n");
  printf("    when the --url argument is present.\n");
  printf("    --url <target URL>             Target for HTTP POSTs.\n");
  printf("    --header <name:value>          Adds HTTP header and value.\n");
  printf("                                   Sent with all POSTs.\n");
  printf("    --form_post                    Send WebM chunks as file data\n");
  printf("                                   in a form (a la RFC 1867).\n");
  printf("    --var <name:value>             Adds form variable and value.\n");
  printf("                                   Sent with all POSTs.\n");
  printf("    --session-id                   Session identifier. Generated\n");
  printf("                                   for you if not specified.\n");
  printf("  Audio source configuration options:\n");
  printf("    --adisable                     Disable audio capture.\n");
  printf("    --amanual                      Attempt manual configuration.\n");
  printf("    --achannels <channels>         Number of audio channels.\n");
  printf("    --arate <sample rate>          Audio sample rate.\n");
  printf("    --asize <sample size>          Audio bits per sample.\n");
  printf("  Vorbis encoder options:\n");
  printf("    --vorbis_bitrate <kbps>            Average bitrate.\n");
  printf("    --vorbis_minimum_bitrate <kbps>    Minimum bitrate.\n");
  printf("    --vorbis_maximum_bitrate <kbps>    Maximum bitrate.\n");
  printf("    --vorbis_disable_vbr               Disable VBR mode when\n");
  printf("                                       specifying only an average\n");
  printf("                                       bitrate.\n");
  printf("    --vorbis_iblock_bias <-15.0-0.0>   Impulse block bias.\n");
  printf("    --vorbis_lowpass_frequency <2-99>  Hard-low pass frequency.\n");
  printf("  Video source configuration options:\n");
  printf("    --vdisable                         Disable video capture.\n");
  printf("    --vmanual                          Attempt manual\n");
  printf("                                       configuration.\n");
  printf("    --vwidth <width>                   Width in pixels.\n");
  printf("    --vheight <height>                 Height in pixels.\n");
  printf("    --vframe_rate <width>              Frames per second.\n");
  printf("  VPx encoder options:\n");
  printf("    --vpx_bitrate <kbps>               Video bitrate.\n");
  printf("    --vpx_codec <codec>                Video codec, vp8 or vp9.\n");
  printf("                                       The default codec is vp8.\n");
  printf("    --vpx_decimate <decimate factor>   FPS reduction factor.\n");
  printf("    --vpx_keyframe_interval <milliseconds>  Time between\n");
  printf("                                            keyframes.\n");
  printf("    --vpx_min_q <min q value>          Quantizer minimum.\n");
  printf("    --vpx_max_q <max q value>          Quantizer maximum.\n");
  printf("    --vpx_noise_sensitivity <0-1>      Blurs adjacent frames to\n");
  printf("                                       reduce the noise level of\n");
  printf("                                       input video.\n");
  printf("    --vpx_static_threshold <threshold> Static threshold.\n");
  printf("    --vpx_speed <speed value>          Speed.\n");
  printf("    --vpx_threads <num threads>        Number of encode threads.\n");
  printf("    --vpx_overshoot <percent>          Overshoot percentage.\n");
  printf("    --vpx_undershoot <percent>         Undershoot percentage.\n");
  printf("    --vpx_max_buffer <length>          Client buffer length (ms).\n");
  printf("    --vpx_init_buffer <length>         Play start length (ms).\n");
  printf("    --vpx_opt_buffer <length>          Optimal length (ms).\n");
  printf("    --vpx_max_kf_bitrate <percent>     Max keyframe bitrate.\n");
  printf("    --vpx_sharpness <0-7>              Loop filter sharpness.\n");
  printf("    --vpx_error_resilience             Enables error resilience.\n");
  printf("  VP8 specific encoder options:\n");
  printf("    --vp8_token_partitions <0-3>       Number of token\n");
  printf("                                       partitions.\n");
  printf("  VP9 specific encoder options:\n");
  printf("    --vp9_aq_mode <0-3>                Adaptive quant mode:\n");
  printf("                                       0: off\n");
  printf("                                       1: variance\n");
  printf("                                       2: complexity\n");
  printf("                                       3: cyclic refresh\n");
  printf("                                         3 is the default.\n");
  printf("    --vp9_gf_cbr_boost <percent>       Golden frame bitrate\n");
  printf("                                       boost.\n");
  printf("    --vp9_tile_cols <cols>             Number of tile columns\n");
  printf("                                       expressed in log2 units:\n");
  printf("                                         0 = 1 tile column\n");
  printf("                                         1 = 2 tile columns\n");
  printf("                                         2 = 4 tile columns\n");
  printf("                                         .....\n");
  printf("                                         6 = 64 tile columns\n");
  printf("                                       Image size controls max\n");
  printf("                                       tile count; min tile width\n");
  printf("                                       is 256 while max is 4096\n");
  printf("    --vp9_disable_fpd                  Disables frame parallel\n");
  printf("                                       decoding.\n");
}

// Parses name value pairs in the format name:value from |unparsed_entries|,
// and stores results in |out_map|.
int StoreStringMapEntries(const StringVector& unparsed_entries,
                          std::map<std::string, std::string>* out_map) {
  using std::string;
  using std::vector;
  StringVector::const_iterator entry_iter = unparsed_entries.begin();
  while (entry_iter != unparsed_entries.end()) {
    const string& entry = *entry_iter;
    size_t sep = entry.find(":");

    if (sep == string::npos) {
      // bad header (missing separator, no value)
      LOG(ERROR) << "ERROR: cannot parse entry, should be name:value, got="
                 << entry.c_str();
      return kBadFormat;
    }

    (*out_map)[entry.substr(0, sep).c_str()] = entry.substr(sep + 1);
    ++entry_iter;
  }
  return kSuccess;
}

// Returns true when |arg_index| + 1 is <= |argc|, and |argv[arg_index+1]| is
// non-null. Command line parser helper function.
bool ArgHasValue(int arg_index, int argc, const char** argv) {
  const int val_index = arg_index + 1;
  const bool has_value = ((val_index < argc) && (argv[val_index] != NULL));
  if (!has_value) {
    LOG(WARNING) << "argument missing value: " << argv[arg_index];
  }
  return has_value;
}

// Parses command line and stores user settings.
void ParseCommandLine(int argc, const char** argv, WebmEncoderConfig* config) {
  StringVector unparsed_headers;
  StringVector unparsed_vars;
  webmlive::HttpUploaderSettings& uploader_settings = config->uploader_settings;
  webmlive::WebmEncoderConfig& enc_config = config->enc_config;
  config->uploader_settings.post_mode = webmlive::HTTP_POST;
  for (int i = 1; i < argc; ++i) {
    if (!strcmp("-h", argv[i]) || !strcmp("-?", argv[i]) ||
        !strcmp("--help", argv[i])) {
      Usage(argv);
      exit(EXIT_SUCCESS);
    } else if (!strcmp("--disable_file_output", argv[i])) {
      config->enable_file_output = false;
    } else if (!strcmp("--disable_http_upload", argv[i])) {
      config->enable_http_upload = false;
    }

    //
    // DASH encoder options.
    //
    else if (!strcmp("--dash", argv[i])) {
      enc_config.dash_encode = true;
    } else if (!strcmp("--dash_dir", argv[i]) && ArgHasValue(i, argc, argv)) {
      enc_config.dash_dir = argv[++i];
      const char last_char = enc_config.dash_dir[enc_config.dash_dir.length()];
      if (last_char != '/' && last_char != '\\') {
        enc_config.dash_dir.append("/");
      }
    } else if (!strcmp("--dash_name", argv[i]) && ArgHasValue(i, argc, argv)) {
      enc_config.dash_name = argv[++i];
    } else if (!strcmp("--dash_start_number", argv[i]) &&
               ArgHasValue(i, argc, argv)) {
      enc_config.dash_start_number = argv[++i];
    }

    //
    // HTTP uploader options.
    //
    else if (!strcmp("--url", argv[i]) && ArgHasValue(i, argc, argv)) {
      uploader_settings.target_url = argv[++i];
    } else if (!strcmp("--header", argv[i]) && ArgHasValue(i, argc, argv)) {
      unparsed_headers.push_back(argv[++i]);
    } else if (!strcmp("--form_post", argv[i]) && ArgHasValue(i, argc, argv)) {
      uploader_settings.post_mode = webmlive::HTTP_FORM_POST;
    } else if (!strcmp("--var", argv[i]) && ArgHasValue(i, argc, argv)) {
      unparsed_vars.push_back(argv[++i]);
    } else if (!strcmp("--session_id", argv[i]) && ArgHasValue(i, argc, argv)) {
      uploader_settings.session_id = argv[++i];
    }

    //
    // Audio source configuration options.
    //
    else if (!strcmp("--adev", argv[i]) && ArgHasValue(i, argc, argv)) {
      enc_config.audio_device_name = argv[++i];
    } else if (!strcmp("--adevidx", argv[i]) && ArgHasValue(i, argc, argv)) {
      enc_config.audio_device_index = strtol(argv[++i], NULL, 10);
    } else if (!strcmp("--achannels", argv[i]) && ArgHasValue(i, argc, argv)) {
      enc_config.requested_audio_config.channels =
          static_cast<uint16>(strtol(argv[++i], NULL, 10));
    } else if (!strcmp("--adisable", argv[i])) {
      enc_config.disable_audio = true;
    } else if (!strcmp("--amanual", argv[i])) {
      enc_config.ui_opts.manual_audio_config = true;
    } else if (!strcmp("--arate", argv[i]) && ArgHasValue(i, argc, argv)) {
      enc_config.requested_audio_config.sample_rate =
          strtol(argv[++i], NULL, 10);
    } else if (!strcmp("--asize", argv[i]) && ArgHasValue(i, argc, argv)) {
      enc_config.requested_audio_config.bits_per_sample =
          static_cast<uint16>(strtol(argv[++i], NULL, 10));
    }

    //
    // Video source configuration options.
    //
    else if (!strcmp("--vdisable", argv[i])) {
      enc_config.disable_video = true;
    } else if (!strcmp("--vdev", argv[i]) && ArgHasValue(i, argc, argv)) {
      enc_config.video_device_name = argv[++i];
    } else if (!strcmp("--vdevidx", argv[i]) && ArgHasValue(i, argc, argv)) {
      enc_config.video_device_index = strtol(argv[++i], NULL, 10);
    } else if (!strcmp("--vmanual", argv[i])) {
      enc_config.ui_opts.manual_video_config = true;
    } else if (!strcmp("--vwidth", argv[i]) && ArgHasValue(i, argc, argv)) {
      enc_config.requested_video_config.width = strtol(argv[++i], NULL, 10);
    } else if (!strcmp("--vheight", argv[i]) && ArgHasValue(i, argc, argv)) {
      enc_config.requested_video_config.height = strtol(argv[++i], NULL, 10);
    } else if (!strcmp("--vframe_rate", argv[i]) &&
               ArgHasValue(i, argc, argv)) {
      enc_config.requested_video_config.frame_rate = strtod(argv[++i], NULL);
    }

    //
    // Vorbis encoder options.
    //
    else if (!strcmp("--vorbis_bitrate", argv[i]) &&
             ArgHasValue(i, argc, argv)) {
      enc_config.vorbis_config.average_bitrate = strtol(argv[++i], NULL, 10);
    } else if (!strcmp("--vorbis_minimum_bitrate", argv[i]) &&
               ArgHasValue(i, argc, argv)) {
      enc_config.vorbis_config.minimum_bitrate = strtol(argv[++i], NULL, 10);
    } else if (!strcmp("--vorbis_maximum_bitrate", argv[i]) &&
               ArgHasValue(i, argc, argv)) {
      enc_config.vorbis_config.maximum_bitrate = strtol(argv[++i], NULL, 10);
    } else if (!strcmp("--vorbis_disable_vbr", argv[i])) {
      enc_config.vorbis_config.bitrate_based_quality = false;
    } else if (!strcmp("--vorbis_iblock_bias", argv[i]) &&
               ArgHasValue(i, argc, argv)) {
      enc_config.vorbis_config.impulse_block_bias = strtod(argv[++i], NULL);
    } else if (!strcmp("--vorbis_lowpass_frequency", argv[i]) &&
               ArgHasValue(i, argc, argv)) {
      enc_config.vorbis_config.lowpass_frequency = strtod(argv[++i], NULL);
    }

    //
    // VPx encoder options.
    else if (!strcmp("--vpx_keyframe_interval", argv[i]) &&
             ArgHasValue(i, argc, argv)) {
      enc_config.vpx_config.keyframe_interval = strtol(argv[++i], NULL, 10);
    } else if (!strcmp("--vpx_bitrate", argv[i]) &&
               ArgHasValue(i, argc, argv)) {
      enc_config.vpx_config.bitrate = strtol(argv[++i], NULL, 10);
    } else if (!strcmp("--vpx_codec", argv[i]) && ArgHasValue(i, argc, argv)) {
      std::string vpx_codec_value = argv[++i];
      if (vpx_codec_value == kCodecVp8)
        enc_config.vpx_config.codec = webmlive::kVideoFormatVP8;
      else if (vpx_codec_value == kCodecVp9)
        enc_config.vpx_config.codec = webmlive::kVideoFormatVP9;
      else
        LOG(ERROR) << "Invalid --vpx_codec value: " << vpx_codec_value;
    } else if (!strcmp("--vpx_decimate", argv[i]) &&
               ArgHasValue(i, argc, argv)) {
      enc_config.vpx_config.decimate = strtol(argv[++i], NULL, 10);
    } else if (!strcmp("--vpx_min_q", argv[i]) && ArgHasValue(i, argc, argv)) {
      enc_config.vpx_config.min_quantizer = strtol(argv[++i], NULL, 10);
    } else if (!strcmp("--vpx_max_q", argv[i]) && ArgHasValue(i, argc, argv)) {
      enc_config.vpx_config.max_quantizer = strtol(argv[++i], NULL, 10);
    } else if (!strcmp("--vpx_noise_sensitivity", argv[i]) &&
               ArgHasValue(i, argc, argv)) {
      enc_config.vpx_config.noise_sensitivity = strtol(argv[++i], NULL, 10);
    } else if (!strcmp("--vpx_speed", argv[i]) && ArgHasValue(i, argc, argv)) {
      enc_config.vpx_config.speed = strtol(argv[++i], NULL, 10);
    } else if (!strcmp("--vpx_static_threshold", argv[i]) &&
               ArgHasValue(i, argc, argv)) {
      enc_config.vpx_config.static_threshold = strtol(argv[++i], NULL, 10);
    } else if (!strcmp("--vpx_threads", argv[i]) &&
               ArgHasValue(i, argc, argv)) {
      enc_config.vpx_config.thread_count = strtol(argv[++i], NULL, 10);
    } else if (!strcmp("--vpx_overshoot", argv[i]) &&
               ArgHasValue(i, argc, argv)) {
      enc_config.vpx_config.overshoot = strtol(argv[++i], NULL, 10);
    } else if (!strcmp("--vpx_undershoot", argv[i]) &&
               ArgHasValue(i, argc, argv)) {
      enc_config.vpx_config.undershoot = strtol(argv[++i], NULL, 10);
    } else if (!strcmp("--vpx_max_buffer", argv[i]) &&
               ArgHasValue(i, argc, argv)) {
      enc_config.vpx_config.total_buffer_time = strtol(argv[++i], NULL, 10);
    } else if (!strcmp("--vpx_init_buffer", argv[i]) &&
               ArgHasValue(i, argc, argv)) {
      enc_config.vpx_config.initial_buffer_time = strtol(argv[++i], NULL, 10);
    } else if (!strcmp("--vpx_opt_buffer", argv[i]) &&
               ArgHasValue(i, argc, argv)) {
      enc_config.vpx_config.optimal_buffer_time = strtol(argv[++i], NULL, 10);
    } else if (!strcmp("--vpx_max_kf_bitrate", argv[i]) &&
               ArgHasValue(i, argc, argv)) {
      enc_config.vpx_config.max_keyframe_bitrate = strtol(argv[++i], NULL, 10);
    } else if (!strcmp("--vpx_sharpness", argv[i]) &&
               ArgHasValue(i, argc, argv)) {
      enc_config.vpx_config.sharpness = strtol(argv[++i], NULL, 10);
    } else if (!strcmp("--vpx_error_resilience", argv[i])) {
      enc_config.vpx_config.error_resilient = true;
    }

    //
    // VP8 specific encoder options.
    //
    else if (!strcmp("--vp8_token_partitions", argv[i]) &&
             ArgHasValue(i, argc, argv)) {
      enc_config.vpx_config.token_partitions = strtol(argv[++i], NULL, 10);
    }

    //
    // VP9 specific encoder options.
    //
    else if (!strcmp("--vp9_aq_mode", argv[i]) && ArgHasValue(i, argc, argv)) {
      enc_config.vpx_config.adaptive_quantization_mode =
          strtol(argv[++i], NULL, 10);
    } else if (!strcmp("--vp9_gf_cbr_boost", argv[i]) &&
               ArgHasValue(i, argc, argv)) {
      enc_config.vpx_config.goldenframe_cbr_boost = strtol(argv[++i], NULL, 10);
    } else if (!strcmp("--vp9_tile_cols", argv[i]) &&
               ArgHasValue(i, argc, argv)) {
      enc_config.vpx_config.tile_columns = strtol(argv[++i], NULL, 10);
    } else if (!strcmp("--vp9_disable_fpd", argv[i]) &&
               ArgHasValue(i, argc, argv)) {
      enc_config.vpx_config.frame_parallel_mode = false;
    } else {
      LOG(WARNING) << "argument unknown or unparseable: " << argv[i];
    }
  }

  // Store user HTTP headers.
  StoreStringMapEntries(unparsed_headers, &uploader_settings.headers);

  // Store user form variables.
  StoreStringMapEntries(unparsed_vars, &uploader_settings.form_variables);
}

// Calls |Init| and |Run| on |ptr_writer| to start the file writer thread, which
// writes buffers when |WriteData| is called on the writer via |DataSink|.
bool StartWriter(WebmEncoderConfig* ptr_config,
                 webmlive::FileWriter* ptr_writer,
                 webmlive::DataSink* ptr_data_sink) {
  if (!ptr_writer->Init(ptr_config->enc_config.dash_encode,
                        ptr_config->enc_config.dash_dir)) {
    LOG(ERROR) << "writer Init failed.";
    return false;
  }

  // Run the writer (it goes idle and waits for a buffer).
  if (!ptr_writer->Run()) {
    LOG(ERROR) << "writer Run failed.";
    return false;
  }
  ptr_data_sink->AddDataSink(ptr_writer);
  return true;
}

// Calls |Init| and |Run| on |uploader| to start the uploader thread, which
// uploads buffers when |UploadBuffer| is called on the uploader.
bool StartUploader(WebmEncoderConfig* ptr_config,
                   webmlive::HttpUploader* ptr_uploader,
                   webmlive::DataSink* ptr_data_sink) {
  if (ptr_config->uploader_settings.session_id.empty()) {
    ptr_config->uploader_settings.session_id =
        webmlive::LocalDateString() + webmlive::LocalTimeString();
  }
  if (!ptr_uploader->Init(ptr_config->uploader_settings)) {
    LOG(ERROR) << "uploader Init failed.";
    return false;
  }

  // Run the uploader (it goes idle and waits for a buffer).
  if (!ptr_uploader->Run()) {
    LOG(ERROR) << "uploader Run failed.";
    return false;
  }
  ptr_data_sink->AddDataSink(ptr_uploader);
  return true;
}

int EncoderMain(WebmEncoderConfig* ptr_config) {
  webmlive::WebmEncoderConfig& enc_config = ptr_config->enc_config;
  webmlive::FileWriter file_writer;
  webmlive::HttpUploader uploader;
  webmlive::DataSink data_sink;

  if (!ptr_config->enable_file_output && !ptr_config->enable_http_upload) {
    LOG(ERROR) << "File output or HTTP upload must be enabled.";
    return EXIT_FAILURE;
  }

  // Init the WebM encoder.
  webmlive::WebmEncoder encoder;
  int status = encoder.Init(enc_config, &data_sink);
  if (status) {
    LOG(ERROR) << "WebmEncoder Run failed, status=" << status;
    return EXIT_FAILURE;
  }

  // Start the file writer thread.
  if (ptr_config->enable_file_output &&
      !StartWriter(ptr_config, &file_writer, &data_sink)) {
    LOG(ERROR) << "start_writer failed.";
    return EXIT_FAILURE;
  }

  // Start the uploader thread.
  if (ptr_config->enable_http_upload &&
      !StartUploader(ptr_config, &uploader, &data_sink)) {
    LOG(ERROR) << "start_uploader failed.";
    return EXIT_FAILURE;
  }

  // Start the WebM encoder.
  status = encoder.Run();
  if (status) {
    LOG(ERROR) << "start_encoder failed, status=" << status;
    uploader.Stop();
    return EXIT_FAILURE;
  }

  webmlive::HttpUploaderStats stats;
  printf("\nPress the any key to quit...\n");

  while (!_kbhit()) {
    // Output current duration and upload progress
    if (uploader.GetStats(&stats)) {
      printf("\rencoded duration: %04f seconds, uploaded: %I64d @ %d kBps",
             (encoder.encoded_duration() / 1000.0),
             stats.bytes_sent_current + stats.total_bytes_uploaded,
             static_cast<int>(stats.bytes_per_second / 1000));
    }
    Sleep(100);
  }

  LOG(INFO) << "stopping encoder...";
  encoder.Stop();
  if (ptr_config->enable_http_upload) {
    LOG(INFO) << "stopping uploader...";
    uploader.Stop();
  }
  if (ptr_config->enable_file_output) {
    LOG(INFO) << "stopping file writer...";
    file_writer.Stop();
  }

  return EXIT_SUCCESS;
}

int main(int argc, const char** argv) {
  google::InitGoogleLogging(argv[0]);
  WebmEncoderConfig config;
  ParseCommandLine(argc, argv, &config);
  const int exit_code = EncoderMain(&config);
  google::ShutdownGoogleLogging();
  return exit_code;
}
