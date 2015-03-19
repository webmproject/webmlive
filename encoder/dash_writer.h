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

class AdaptationSet {
public:
 enum MediaType {
   kAudio = 1,
   kVideo = 2,
 };

 AdaptationSet();
 virtual ~AdaptationSet() {}

 bool enabled;
 MediaType media_type;

 bool segment_alignment;
 bool bitstream_switching;

 // ContentComponent properties.
 std::string cc_id;
 std::string content_type;

 // SegmentTemplate properties.
 int timescale;
 int chunk_duration;
 std::string media;
 int start_number;
 std::string initialization;

 // Representation properties.
 // TODO(tomfinegan): Support multiple Representation elements.
 std::string rep_id;
 std::string mimetype;
 std::string codecs;
 int start_with_sap;
 int bandwidth;
};

class AudioAdaptationSet : public AdaptationSet {
 public:
  AudioAdaptationSet();
  virtual ~AudioAdaptationSet() {}

  // Audio AdaptationSet properties.
  int audio_sampling_rate;

  // AudioChannelConfiguration.
  std::string scheme_id_uri;
  int value;  // Audio channels.
};

class VideoAdaptationSet : public AdaptationSet {
 public:
  VideoAdaptationSet();
  virtual ~VideoAdaptationSet() {}

  // Video AdaptationSet properties.
  int max_width;
  int max_height;
  int max_frame_rate;

  // Representation.
  int width;
  int height;
  int frame_rate;
};

struct DashConfig {
  DashConfig();

  // MPD properties.
  std::string type;
  int min_buffer_time;
  int media_presentation_duration;

  // Period properties.
  int start_time;
  int period_duration;

  // Audio/Video adaptation sets.
  // TODO(tomfinegan): Support multiple adaptation sets per media type.
  AudioAdaptationSet audio_as;
  VideoAdaptationSet video_as;
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
            const WebmEncoderConfig& webm_config);

  // Writes the DASH manifest built from |config| to |manifest|. Returns true
  // when successful.
  bool WriteManifest(std::string* manifest);

  // Returns a string suitable for identifying a chunk.
  void IdForChunk(AdaptationSet::MediaType media_type, int64 chunk_num,
                  std::string* chunk_id) const;
 private:
  void WriteAudioAdaptationSet(std::string* adaptation_set);
  void WriteVideoAdaptationSet(std::string* adaptation_set);

  void IncreaseIndent();
  void DecreaseIndent();
  void ResetIndent();

  bool initialized_;
  DashConfig config_;
  std::string indent_;
  std::string name_;
  std::string id_;
};

}  // namespace webmlive

#endif  // WEBMLIVE_ENCODER_DASH_WRITER_H_
