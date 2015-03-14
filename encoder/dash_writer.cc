// Copyright (c) 2015 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "encoder/dash_writer.h"

#include <ios>
#include <sstream>

#include "glog/logging.h"

namespace webmlive {
const char kIndentStep[] = "  ";

// Default values for DashConfig. Time values in seconds unless otherwise noted.
// TODO(tomfinegan): Not sure if default belongs in the name for schema and
// profiles here; should these be configurable?
const char kDefaultSchema[] = "urn:mpeg:dash:schema:mpd:2011";
const int kDefaultMinBufferTime = 1;
const int kDefaultMediaPresentationDuration = 36000;  // 10 hours.
const char kDefaultType[] = "static";
const char kDefaultProfiles[] = "urn:mpeg:dash:profile:isoff-live:2011";
const int kDefaultStartTime = 0;
const int kDefaultMaxWidth = 1920;
const int kDefaultMaxHeight = 1080;
const int kDefaultMaxFrameRate = 60;
const int kDefaultContentComponentId = 1;
const char kDefaultContentComponentType[] = "video";
const int kDefaultPeriodDuration = kDefaultMediaPresentationDuration;
const int kDefaultTimescale = 1000;  // milliseconds.
const int kDefaultChunkDuration = 5000;  // milliseconds.
const int kDefaultStartNumber = 1;
const char kDefaultMimeType[] = "video/webm";
const char kDefaultCodecs[] = "vp9";
const int kDefaultStartWithSap = 1;
const int kDefaultBandwidth = 1000000;  // Bits.
const int kDefaultFrameRate = 30;

//
// DashConfig
//
DashConfig::DashConfig()
      : min_buffer_time(kDefaultMinBufferTime),
        media_presentation_duration(kDefaultMediaPresentationDuration),
        start_time(kDefaultStartTime),
        period_duration(kDefaultPeriodDuration),
        segment_alignment(true),
        type(kDefaultType),
        bitstream_switching(false),
        max_width(kDefaultMaxWidth),
        max_height(kDefaultMaxHeight),
        max_frame_rate(kDefaultMaxFrameRate),
        cc_id(kDefaultContentComponentId),
        content_type(kDefaultContentComponentType),
        timescale(kDefaultTimescale),
        chunk_duration(kDefaultChunkDuration),
        start_number(kDefaultStartNumber),
        mimetype(kDefaultMimeType),
        codecs(kDefaultCodecs),
        width(kDefaultMaxWidth),
        height(kDefaultMaxHeight),
        start_with_sap(kDefaultStartWithSap),
        bandwidth(kDefaultBandwidth),
        frame_rate(kDefaultFrameRate) {
}

//
// DashWriter
//

bool DashWriter::Init(std::string name, std::string id,
                      const WebmEncoderConfig& webm_config,
                      DashConfig* dash_config) {
  CHECK_NOTNULL(dash_config);
  if (name.empty() || id.empty()) {
    LOG(ERROR) << "name or id empty in DashWriter::Init()";
    return false;
  }

  const char kChunkPattern[] = "_$RepresentationID$_$Number$.chk";
  const char kInitializationPattern[] = "_$RepresentationID$.hdr";
  config_.media = name + kChunkPattern;
  config_.initialization = name + kInitializationPattern;
  config_.rep_id = id;

  config_.width = webm_config.actual_video_config.width;
  config_.height = webm_config.actual_video_config.height;

  if (webm_config.vpx_config.decimate != VpxConfig::kUseDefault) {
    config_.frame_rate = static_cast<int>(
        std::ceil(webm_config.actual_video_config.frame_rate /
                  webm_config.vpx_config.decimate));
  } else {
    config_.frame_rate = static_cast<int>(
        std::ceil(webm_config.actual_video_config.frame_rate));
  }

  if (config_.frame_rate > config_.max_frame_rate) {
    config_.max_frame_rate = config_.frame_rate;
  }

  config_.chunk_duration = webm_config.vpx_config.keyframe_interval;

  *dash_config = config_;
  initialized_ = true;
  return true;
}

bool DashWriter::WriteManifest(const DashConfig& config,
                               std::string* out_manifest) {
  CHECK_NOTNULL(out_manifest);
  if (!initialized_ || config.media.empty() || config.initialization.empty()) {
    LOG(ERROR) << "DashWriter not initialized before call to WriteManifest()";
    return false;
  }

  std::ostringstream manifest;

  // Open the MPD element.
  manifest << "<MPD "
           << "xmlns=\"" << kDefaultSchema << "\" "
           << "type=\"" << config.type << "\" "
           << "minBufferTime=\"PT" << config.min_buffer_time << "S\" "
           << "mediaPresentationDuration=\"PT"
           << config.media_presentation_duration << "\" "
           << "profiles=\"" << kDefaultProfiles << "\">"
           << "\n";
  IncreaseIndent();

  // Open the Period element.
  manifest << indent_
           << "<Period "
           << "start=\"PT" << config.start_time << "S\" "
           << "duration=\"PT" << config.period_duration << "\">"
           << "\n";
  IncreaseIndent();

  // Open the AdaptationSet element.
  manifest << indent_
           << "<AdaptationSet "
           << "segmentAlignment=\""
           << std::boolalpha << config.segment_alignment << "\" "
           << "bitstreamSwitching=\"" << config.bitstream_switching << "\" "
           << "maxWidth=\"" << config.max_width << "\" "
           << "maxHeight=\"" << config.max_height << "\" "
           << "maxFrameRate=\"" << config.max_frame_rate << "\">"
           << "\n";
  IncreaseIndent();

  // Write ContentComponent element.
  manifest << indent_
           << "<ContentComponent "
           << "id=\"" << config.cc_id << "\" "
           << "contentType=\"" << config.content_type << "\"/>"
           << "\n";

  // Write SegmentTemplate element.
  manifest << indent_
           << "<SegmentTemplate "
           << "timescale=\"" << config.timescale << "\" "
           << "duration=\"" << config.chunk_duration << "\" "
           << "media=\"" << config.media << "\" "
           << "startNumber=\"" << config.start_number << "\" "
           << "initialization=\"" << config.initialization << "\"/>"
           << "\n";

  // Write the Representation element.
  manifest << indent_
           << "<Representation "
           << "id=\"" << config.rep_id << "\" "
           << "mimeType=\"" << config.mimetype << "\" "
           << "codecs=\"" << config.codecs << "\" "
           << "width=\"" << config.width << "\" "
           << "height=\"" << config.height << "\" "
           << "startWithSAP=\"" << config.start_with_sap << "\" "
           << "bandwidth=\"" << config.bandwidth << "\" "
           << "framerate=\"" << config.frame_rate << "\" "
           << "></Representation>"
           << "\n";

  // Close open elements.
  DecreaseIndent();
  manifest << indent_ << "</AdaptationSet>\n";
  DecreaseIndent();
  manifest << indent_ << "</Period>\n";
  DecreaseIndent();
  manifest << indent_ << "</MPD>\n";

  LOG(INFO) << "\nmanifest:\n" << manifest.str();
  *out_manifest = manifest.str();
  config_ = config;

  return true;
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
