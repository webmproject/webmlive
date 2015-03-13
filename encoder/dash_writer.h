// Copyright (c) 2015 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef WEBMLIVE_ENCODER_DASH_WRITER_H_
#define WEBMLIVE_ENCODER_DASH_WRITER_H_

#include <string>

#include "encoder/webm_encoder.h"

namespace webmlive {

struct DashConfig{
  // Time values in seconds unless otherwise noted.
  const int kDefaultMinBufferTime = 1;
  const int kDefaultMediaPresentationDuration = 36000;  // 10 hours.
  // TODO(tomfinegan): Not sure if default belongs in the name for schema and
  // profiles here; should these be configurable?
  const static char kDefaultSchema[];
  const static char kDefaultType[];
  const static char kDefaultProfiles[];
  const int kDefaultStartTime = 0;
  const int kDefaultMaxWidth = 1920;
  const int kDefaultMaxHeight = 1080;
  const int kDefaultMaxFrameRate = 60;
  const int kDefaultContentComponentId = 1;
  const static char kDefaultContentComponentType[];
  const int kDefaultPeriodDuration = kDefaultMediaPresentationDuration;
  const int kDefaultTimescale = 1000;  // milliseconds.
  const int kDefaultChunkDuration = 5000;  // milliseconds.
  const int kDefaultStartNumber = 1;
  const static char kDefaultMimeType[];
  const static char kDefaultCodecs[];
  const int kDefaultStartWithSap = 1;
  const int kDefaultBandwidth = 1000000;  // Bits.
  const int kDefaultFrameRate = 30;

  DashConfig();

  // MPD.
  int min_buffer_time;
  int media_presentation_duration;

  // Period.
  int start_time;
  int period_duration;

  // AdaptationSet.
  bool segment_alignment;
  std::string type;
  bool bitstream_switching;
  int max_width;
  int max_height;
  int max_framerate;
  
  // ContentComponent.
  int cc_id;
  std::string content_type;

  // SegmentTemplate.
  int timescale;
  int chunk_duration;
  std::string media;
  int start_number;
  std::string initialization;

  // Representation.
  std::string rep_id;
  std::string mimetype;
  std::string codecs;
  int width;
  int height;
  int start_with_sap;
  int bandwidth;
  int framerate;
};

class DashWriter {
 public:
  DashWriter() : initialized_(false){}
  ~DashWriter();

  // Builds the SegmentTemplate media and initialization strings. Must be called
  // before |WriteManifest()|. Returns true when successful.
  bool Init(std::string name, std::string id);

  // Writes the DASH manifest built from |config| to |manifest|. Returns true
  // when successful.
  bool WriteManifest(const DashConfig& config,
                     std::string* manifest);
 private:
  void IncreaseIndent();
  void DecreaseIndent();
  void ResetIndent();

  bool initialized_;
  std::string indent_;
};

}  // namespace webmlive

#endif  // WEBMLIVE_ENCODER_DASH_WRITER_H_
