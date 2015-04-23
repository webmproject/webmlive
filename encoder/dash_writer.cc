// Copyright (c) 2015 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "encoder/dash_writer.h"

#include <ctime>
#include <ios>
#include <sstream>

#include "glog/logging.h"

#include "encoder/time_util.h"

namespace webmlive {
const char kIndentStep[] = "  ";

// Default values for DashConfig. Time values in seconds unless otherwise noted.
// TODO(tomfinegan): Not sure if default belongs in the name for schema and
// profiles here; should these be configurable?
const char kDefaultSchema[] = "urn:mpeg:dash:schema:mpd:2011";
const int kDefaultMinBufferTime = 1;
const int kDefaultMediaPresentationDuration = 36000;  // 10 hours.
const char kDefaultType[] = "dynamic";
const char kDefaultProfiles[] = "urn:mpeg:dash:profile:isoff-live:2011";
const int kDefaultStartTime = 0;
const int kDefaultMaxWidth = 1920;
const int kDefaultMaxHeight = 1080;
const int kDefaultMaxFrameRate = 60;
const int kDefaultContentComponentId = 1;
const char kContentComponentTypeAudio[] = "audio";
const char kContentComponentTypeVideo[] = "video";
const int kDefaultPeriodDuration = kDefaultMediaPresentationDuration;
const int kDefaultTimescale = 1000;  // milliseconds.
const int kDefaultChunkDuration = 5000;  // milliseconds.
const std::string kDefaultStartNumber = "1";
const int kDefaultStartWithSap = 1;
const int kDefaultBandwidth = 1000000;  // Bits.
const int kDefaultFrameRate = 30;
const int kDefaultAudioSampleRate = 44100;
const int kDefaultAudioChannels = 2;

const char kAudioMimeType[] = "audio/webm";
const char kVideoMimeType[] = "video/webm";
const char kAudioCodecs[] = "vorbis";
const char kVideoCodecs[] = "vp9";
const char kAudioId[] = "1";
const char kVideoId[] = "2";

// Base strings for initialization and chunk names.
const char kChunkPattern[] = "_$RepresentationID$_$Number$.chk";
const char kInitializationPattern[] = "_$RepresentationID$.hdr";

const char kAudioSchemeUri[] =
  "urn:mpeg:dash:23003:3:audio_channel_configuration:2011";

// %Y - year
// %m - month, zero padded (01-12)
// %d - day of month, zero padded (01-31).
// %H - hour, zero padded, 24 hour clock (00-23)
// %M - minute, zero padded (00-59)
// %S - second, zero padded (00-61)
const char kAvailabilityStartTimeFormat[] = "%Y-%m-%dT%H:%M:%SZ";

//
// AdaptationSet
//
AdaptationSet::AdaptationSet()
    : enabled(false),
      media_type(kVideo),
      segment_alignment(true),
      bitstream_switching(false),
      cc_id(kVideoId),
      content_type(kContentComponentTypeVideo),
      timescale(kDefaultTimescale),
      chunk_duration(kDefaultChunkDuration),
      start_number(kDefaultStartNumber),
      start_with_sap(kDefaultStartWithSap),
      bandwidth(kDefaultBandwidth) {}

//
// AudioAdaptationSet
//
AudioAdaptationSet::AudioAdaptationSet()
    : audio_sampling_rate(kDefaultAudioSampleRate),
      scheme_id_uri(kAudioSchemeUri),
      value(kDefaultAudioChannels) {
  media_type = kAudio;
  content_type = kContentComponentTypeAudio;
  mimetype = kAudioMimeType;
  codecs = kAudioCodecs;
}

//
// VideoAdaptationSet
//
VideoAdaptationSet::VideoAdaptationSet()
    : max_width(kDefaultMaxWidth),
      max_height(kDefaultMaxHeight),
      max_frame_rate(kDefaultMaxFrameRate),
      width(kDefaultMaxWidth),
      height(kDefaultMaxHeight),
      frame_rate(kDefaultFrameRate) {
  media_type = kVideo;
  cc_id = kAudioId;
  mimetype = kVideoMimeType;
  codecs = kVideoCodecs;
}

//
// DashConfig
//
DashConfig::DashConfig()
      : type(kDefaultType),
        min_buffer_time(kDefaultMinBufferTime),
        media_presentation_duration(kDefaultMediaPresentationDuration),
        start_time(kDefaultStartTime),
        period_duration(kDefaultPeriodDuration) {}

//
// DashWriter
//

bool DashWriter::Init(const WebmEncoderConfig& webm_config) {
  if (webm_config.dash_name.empty()) {
    LOG(ERROR) << "name empty in DashWriter::Init()";
    return false;
  }

  name_ = webm_config.dash_name;

  if (!webm_config.disable_audio) {
    config_.audio_as.enabled = true;
    config_.audio_as.bandwidth =
        webm_config.vorbis_config.average_bitrate * 1000;
    config_.audio_as.media = name_ + kChunkPattern;
    config_.audio_as.initialization = name_ + kInitializationPattern;
    config_.audio_as.rep_id = kAudioId;
    config_.audio_as.audio_sampling_rate =
        webm_config.actual_audio_config.sample_rate;
    config_.audio_as.value = webm_config.actual_audio_config.channels;
    config_.audio_as.start_number = webm_config.dash_start_number;
  }
  if (!webm_config.disable_video) {
    config_.video_as.enabled = true;
    config_.video_as.bandwidth = webm_config.vpx_config.bitrate * 1000;
    config_.video_as.media = name_ + kChunkPattern;
    config_.video_as.initialization = name_ + kInitializationPattern;
    config_.video_as.rep_id = kVideoId;
    config_.video_as.width = webm_config.actual_video_config.width;
    config_.video_as.height = webm_config.actual_video_config.height;
    config_.video_as.start_number = webm_config.dash_start_number;

    if (webm_config.vpx_config.decimate != VpxConfig::kUseDefault) {
      config_.video_as.frame_rate = static_cast<int>(
          std::ceil(webm_config.actual_video_config.frame_rate /
                    webm_config.vpx_config.decimate));
    } else {
      config_.video_as.frame_rate = static_cast<int>(
          std::ceil(webm_config.actual_video_config.frame_rate));
    }

    if (config_.video_as.frame_rate > config_.video_as.max_frame_rate) {
      config_.video_as.max_frame_rate = config_.video_as.frame_rate;
    }
  }

  config_.audio_as.chunk_duration = webm_config.vpx_config.keyframe_interval;
  config_.video_as.chunk_duration = webm_config.vpx_config.keyframe_interval;

  initialized_ = true;
  return true;
}

bool DashWriter::WriteManifest(std::string* out_manifest) {
  CHECK_NOTNULL(out_manifest);
  if (!initialized_) {
    LOG(ERROR) << "DashWriter not initialized before call to WriteManifest()";
    return false;
  }

  std::ostringstream manifest;

  manifest << "<?xml version=\"1.0\"?>\n";

  time_t raw_time = time(NULL);

  // Open the MPD element.
  manifest << "<MPD "
           << "xmlns=\"" << kDefaultSchema << "\" "
           << "type=\"" << config_.type << "\" "
           << "availabilityStartTime=\""
           << StrFTime(gmtime(&raw_time), kAvailabilityStartTimeFormat)
           << "\" "
           << "minBufferTime=\"PT" << config_.min_buffer_time << "S\" "
           << "mediaPresentationDuration=\"PT"
           << config_.media_presentation_duration << "S\" "
           << "profiles=\"" << kDefaultProfiles << "\">"
           << "\n";
  IncreaseIndent();

  // Open the Period element.
  manifest << indent_
           << "<Period "
           << "start=\"PT" << config_.start_time << "S\" "
           << "duration=\"PT" << config_.period_duration << "S\">"
           << "\n";
  IncreaseIndent();

  if (config_.audio_as.enabled) {
    std::string audio_as;
    WriteAudioAdaptationSet(&audio_as);
    manifest << audio_as;
  }

  if (config_.video_as.enabled) {
    std::string video_as;
    WriteVideoAdaptationSet(&video_as);
    manifest << video_as;
  }

  // Close open elements.
  DecreaseIndent();
  manifest << indent_ << "</Period>\n";
  DecreaseIndent();
  manifest << indent_ << "</MPD>\n";

  LOG(INFO) << "\nmanifest:\n" << manifest.str();
  *out_manifest = manifest.str();
  return true;
}

std::string DashWriter::IdForChunk(AdaptationSet::MediaType media_type,
                                   int64 chunk_num) const {
  CHECK(initialized_);
  std::string initialization;
  std::string media;
  if (media_type == AdaptationSet::kAudio) {
    initialization = name_ + "_" + kAudioId + ".hdr";
    media  = name_ + "_" + kAudioId + "_";
  } else {
    initialization = name_ + "_" + kVideoId + ".hdr";
    media  = name_ + "_" + kVideoId + "_";
  }

  std::ostringstream id;
  if (chunk_num == 0) {
    id << initialization << "";
  } else {
    id << media << chunk_num << ".chk";
  }
  return id.str();
}

void DashWriter::WriteAudioAdaptationSet(std::string* adaptation_set) {
  CHECK_NOTNULL(adaptation_set);
  std::ostringstream a_stream;
  const AudioAdaptationSet& audio_as = config_.audio_as;

  // Open the AdaptationSet element.
  a_stream << indent_
           << "<AdaptationSet "
           << "segmentAlignment=\""
           << std::boolalpha << audio_as.segment_alignment << "\" "
           << "audioSamplingRate=\"" << audio_as.audio_sampling_rate << "\" "
           << "bitstreamSwitching=\"" << audio_as.bitstream_switching << "\">"
           << "\n";
  IncreaseIndent();

  // Write AudioChannelConfiguration element.
  a_stream << indent_
           << "<AudioChannelConfiguration "
           << "schemeIdUri=\"" << audio_as.scheme_id_uri << "\" "
           << "value=\"" << audio_as.value << "\">"
           << "</AudioChannelConfiguration>"
           << "\n";

  // Write ContentComponent element.
  a_stream << indent_
           << "<ContentComponent "
           << "id=\"" << audio_as.cc_id << "\" "
           << "contentType=\"" << audio_as.content_type << "\"/>"
           << "\n";

  // Write SegmentTemplate element.
  a_stream << indent_
           << "<SegmentTemplate "
           << "timescale=\"" << audio_as.timescale << "\" "
           << "duration=\"" << audio_as.chunk_duration << "\" "
           << "media=\"" << audio_as.media << "\" "
           << "startNumber=\"" << audio_as.start_number << "\" "
           << "initialization=\"" << audio_as.initialization << "\"/>"
           << "\n";

  // Write the Representation element.
  a_stream << indent_
           << "<Representation "
           << "id=\"" << audio_as.rep_id << "\" "
           << "mimeType=\"" << audio_as.mimetype << "\" "
           << "codecs=\"" << audio_as.codecs << "\" "
           << "startWithSAP=\"" << audio_as.start_with_sap << "\" "
           << "bandwidth=\"" << audio_as.bandwidth << "\" "
           << "></Representation>"
           << "\n";

  // Close open the AdaptationSet element.
  DecreaseIndent();
  a_stream << indent_ << "</AdaptationSet>\n";
  *adaptation_set = a_stream.str();
}

void DashWriter::WriteVideoAdaptationSet(std::string* adaptation_set) {
  CHECK_NOTNULL(adaptation_set);
  std::ostringstream v_stream;
  const VideoAdaptationSet& video_as = config_.video_as;

  // Open the AdaptationSet element.
  v_stream << indent_
           << "<AdaptationSet "
           << "segmentAlignment=\""
           << std::boolalpha << video_as.segment_alignment << "\" "
           << "bitstreamSwitching=\"" << video_as.bitstream_switching << "\" "
           << "maxWidth=\"" << video_as.max_width << "\" "
           << "maxHeight=\"" << video_as.max_height << "\" "
           << "maxFrameRate=\"" << video_as.max_frame_rate << "\">"
           << "\n";
  IncreaseIndent();

  // Write ContentComponent element.
  v_stream << indent_
           << "<ContentComponent "
           << "id=\"" << video_as.cc_id << "\" "
           << "contentType=\"" << video_as.content_type << "\"/>"
           << "\n";

  // Write SegmentTemplate element.
  v_stream << indent_
           << "<SegmentTemplate "
           << "timescale=\"" << video_as.timescale << "\" "
           << "duration=\"" << video_as.chunk_duration << "\" "
           << "media=\"" << video_as.media << "\" "
           << "startNumber=\"" << video_as.start_number << "\" "
           << "initialization=\"" << video_as.initialization << "\"/>"
           << "\n";

  // Write the Representation element.
  v_stream << indent_
           << "<Representation "
           << "id=\"" << video_as.rep_id << "\" "
           << "mimeType=\"" << video_as.mimetype << "\" "
           << "codecs=\"" << video_as.codecs << "\" "
           << "width=\"" << video_as.width << "\" "
           << "height=\"" << video_as.height << "\" "
           << "startWithSAP=\"" << video_as.start_with_sap << "\" "
           << "bandwidth=\"" << video_as.bandwidth << "\" "
           << "frameRate=\"" << video_as.frame_rate << "\" "
           << "></Representation>"
           << "\n";

  // Close open the AdaptationSet element.
  DecreaseIndent();
  v_stream << indent_ << "</AdaptationSet>\n";
  *adaptation_set = v_stream.str();
}

void DashWriter::IncreaseIndent() {
  indent_ = indent_ + kIndentStep;
}

void DashWriter::DecreaseIndent() {
  std::string indent_step = kIndentStep;
  if (indent_.length() > 0)
    indent_ = indent_.substr(0, indent_.length() - indent_step.length());
}

void DashWriter::ResetIndent() {
  indent_ = "";
}

}  // namespace webmlive
