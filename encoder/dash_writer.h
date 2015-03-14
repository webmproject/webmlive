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

struct DashConfig {
  DashConfig();

  //
  // Each section defines the properties supported for the element named.
  //

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
  int max_frame_rate;

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
  // TODO(tomfinegan): Support multiple Representation elements.
  std::string rep_id;
  std::string mimetype;
  std::string codecs;
  int width;
  int height;
  int start_with_sap;
  int bandwidth;
  int frame_rate;
};

class DashWriter {
 public:
  DashWriter() : initialized_(false){}
  ~DashWriter() {}

  DashConfig config() const { return config_; }
  void config(DashConfig& config) { config_ = config; }

  // Builds the SegmentTemplate media and initialization strings and then stores
  // them in |config|. Must be called before |WriteManifest()|. Returns true
  // when successful.
  bool Init(std::string name, std::string id,
            const WebmEncoderConfig& webm_config, DashConfig* config);

  // Writes the DASH manifest built from |config| to |manifest|. Returns true
  // when successful.
  bool WriteManifest(const DashConfig& config,
                     std::string* manifest);

 private:
  void IncreaseIndent();
  void DecreaseIndent();
  void ResetIndent();

  bool initialized_;
  DashConfig config_;
  std::string indent_;
};

}  // namespace webmlive

#endif  // WEBMLIVE_ENCODER_DASH_WRITER_H_
