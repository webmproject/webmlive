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

const char DashConfig::kDefaultSchema[] = "urn:mpeg:dash:schema:mpd:2011";
const char DashConfig::kDefaultProfiles[] =
    "urn:mpeg:dash:profile:isoff-live:2011";
const char DashConfig::kDefaultType[] = "static";
const char DashConfig::kDefaultContentComponentType[] = "video";
const char DashConfig::kDefaultMimeType[] = "video/webm";
const char DashConfig::kDefaultCodecs[] = "vp9";

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
        max_framerate(kDefaultMaxFrameRate),
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
        framerate(kDefaultFrameRate) {
}

//
// DashWriter
//

bool DashWriter::Init(std::string name, std::string id) {
  if (name.empty() || id.empty()) {
    LOG(ERROR) << "name or id empty in DashWriter::Init()";
    return false;
  }

  initialized_ = true;
  return true;
}

bool DashWriter::WriteManifest(const DashConfig& config,
                               std::string* out_manifest) {
  if (!initialized_) {
    LOG(ERROR) << "DashWriter not initialized before call to WriteManifest()";
    return false;
  }

  std::ostringstream manifest;

  // Open the MPD element.
  manifest << "<MPD "
           << "xmlns=\"" << DashConfig::kDefaultSchema << "\" "
           << "type=\"" << config.type << "\" "
           << "minBufferTime=\"PT" << config.min_buffer_time << "S\" "
           << "mediaPresentationDuration=\"PT" 
           << config.media_presentation_duration << "\" " 
           << "profiles=\"" << DashConfig::kDefaultProfiles << "\">"
           << "\n";
  IncreaseIndent();
  
  // Open the Period element.
  manifest << indent_
           << "<Period "
           << "start=\"PT" << config.start_time << "0S\" "
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
           << "maxFrameRate=\"" << config.max_framerate << "\">"
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
           << "framerate=\"" << config.framerate << "\" "
           << "></Representation>"
           << "\n";
   
  // Close open elements.
  manifest << indent_ << "</AdaptationSet>";
  DecreaseIndent();  
  manifest << indent_ << "</Period>";
  DecreaseIndent();
  manifest << indent_ << "</MPD>";

  ResetIndent();
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