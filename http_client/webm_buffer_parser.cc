// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "webm_buffer_parser.h"

#include "glog/logging.h"
#include "libwebm/mkvparser.hpp"

namespace webmlive {
// Provides a moving window into a buffer, and implements libwebm's IMkvReader
// interface.  |WebmBufferParser| sets the window into a buffer by calling
// |SetBufferWindow|, and libwebm parses the data using the |Read| and |Length|
// methods.
class WebmBufferReader : public mkvparser::IMkvReader {
 public:
  enum {
    kInvalidArg   = WebmBufferParser::kInvalidArg,
    kSuccess      = WebmBufferParser::kSuccess,
    kNeedMoreData = WebmBufferParser::kNeedMoreData,
  };
  WebmBufferReader();
  virtual ~WebmBufferReader();
  // Updates the buffer window and returns |kSuccess|.
  int SetBufferWindow(const uint8* ptr_buffer, int32 length,
                      int64 bytes_consumed);
  // IMkvReader methods.
  virtual int Read(int64 read_pos, long length_requested, uint8* ptr_buf);
  virtual int Length(int64* ptr_total, int64* ptr_available);
 private:
  // Buffer window pointer.
  const uint8* ptr_buffer_;
  // Length of the buffer window to which |ptr_buffer_| provides access.
  int32 length_;
  // Sum of all parsed element lengths.
  int64 bytes_consumed_;
  WEBMLIVE_DISALLOW_COPY_AND_ASSIGN(WebmBufferReader);
};

WebmBufferReader::WebmBufferReader()
    : ptr_buffer_(NULL),
      length_(0),
      bytes_consumed_(0) {
}

WebmBufferReader::~WebmBufferReader() {
}

// Updates the buffer window.
int WebmBufferReader::SetBufferWindow(const uint8* ptr_buffer, int32 length,
                                      int64 bytes_consumed) {
  if (!ptr_buffer || !length) {
    LOG(ERROR) << "invalid arg(s)";
    return kInvalidArg;
  }
  ptr_buffer_ = ptr_buffer;
  length_ = length;
  bytes_consumed_ = bytes_consumed;
  return kSuccess;
}

// Called by libwebm to read data from |ptr_buffer_|.
int WebmBufferReader::Read(int64 read_pos, long length_requested,
                           uint8* ptr_buf) {
  if (!ptr_buf) {
    LOG(ERROR) << "NULL ptr_buf";
    return 0;
  }
  // |read_pos| includes |bytes_consumed_|, which is not going to work with the
  // buffer window-- calculate actual offset within |ptr_buf|.
  const int64 window_pos = read_pos - bytes_consumed_;
  assert(window_pos >= 0);
  // Is enough data in the buffer?
  const int64 bytes_available = length_ - window_pos;
  if (bytes_available < length_requested) {
    // No, not enough data buffered.
    return mkvparser::E_BUFFER_NOT_FULL;
  }
  VLOG(4) << "read_pos=" << read_pos << " window_pos=" << window_pos
          << " length_=" << length_ << " length_requested=" << length_requested
          << " available=" << bytes_available;
  // Yes, there's enough data in the buffer.
  memcpy(ptr_buf, ptr_buffer_ + window_pos, length_requested);
  return kSuccess;
}

// Called by libwebm to obtain length of readable data.  Returns
// |bytes_consumed_| + |length_| as available amount, which is the size of the
// buffer current window plus the sum of the sizes of all parsed elements.
int WebmBufferReader::Length(int64* ptr_total, int64* ptr_available) {
  if (!ptr_total || !ptr_available) {
    LOG(ERROR) << "invalid arg(s)";
    return -1;
  }
  *ptr_total = -1;  // Total file size is unknown.
  *ptr_available = bytes_consumed_ + length_;
  return 0;
}

///////////////////////////////////////////////////////////////////////////////
// WebmBufferParser
//

WebmBufferParser::WebmBufferParser()
    : ptr_cluster_(NULL),
      cluster_parse_offset_(0),
      total_bytes_parsed_(0),
      mode_(kParseModeSegmentHeaders) {
}

WebmBufferParser::~WebmBufferParser() {
}

// Constructs |reader_|.
int WebmBufferParser::Init() {
  reader_.reset(new (std::nothrow) WebmBufferReader());
  if (!reader_) {
    LOG(ERROR) << "out of memory";
    return kOutOfMemory;
  }
  return kSuccess;
}

// Tries to parse data in |buf|.  Sets |ptr_element_size| to the size of the
// parsed element when parsing of the segment headers or a cluster is
// successful.
int WebmBufferParser::Parse(const Buffer& buf, int32* ptr_element_size) {
  if (!ptr_element_size) {
    LOG(ERROR) << "NULL element size pointer!";
    return kInvalidArg;
  }
  if (buf.empty()) {
    return kNeedMoreData;
  }
  // Update |reader_|'s buffer window...
  if (reader_->SetBufferWindow(&buf[0], buf.size(), total_bytes_parsed_)) {
    LOG(ERROR) << "could not update buffer window";
    return kParseError;
  }
  // Try to parse...
  int parse_status = kNeedMoreData;
  switch (mode_) {
    case kParseModeClusters:
      parse_status = ParseCluster(ptr_element_size);
      break;
    case kParseModeSegmentHeaders:
      parse_status = ParseSegmentHeaders(ptr_element_size);
      if (parse_status == kSuccess) {
        // Parsed the segment headers, look for clusters from here on out...
        mode_ = kParseModeClusters;
      }
  }
  return parse_status;
}

// Tries to parse the segment headers, segment info and segment tracks.
// Returns |kNeedMoreData| if unable to parse them.  Returns |kSuccess| and
// sets |ptr_element_size| when successful.
int WebmBufferParser::ParseSegmentHeaders(int32* ptr_element_size) {
  mkvparser::EBMLHeader ebml_header;
  int64 pos = 0;
  int64 parse_status = ebml_header.Parse(reader_.get(), pos);
  if (parse_status) {
    LOG(INFO) << "EBML header parse failed, parse_status=" << parse_status;
    return kNeedMoreData;
  }
  // |pos| is equal to the length of the EBML header; start a running total now
  // since |ebml_header| doesn't store a length.
  int64 headers_length = pos;
  // Create and start parse of the segment...
  mkvparser::Segment* ptr_segment = NULL;
  parse_status = mkvparser::Segment::CreateInstance(reader_.get(), pos,
                                                    ptr_segment);
  if (parse_status) {
    LOG(INFO) << "segment creation failed, parse_status=" << parse_status;
    return kNeedMoreData;
  }
  segment_.reset(ptr_segment);
  // Add the segment header length to the running total. The position argument
  // to the |CreateInstance| call above is not passed by reference (as is the
  // case with |ebml_header|), so |pos| is still correct.
  headers_length += segment_->m_start - pos;
  // |ParseHeaders| reads data until it runs out or finds a cluster. If it
  // finds a cluster, it returns 0 ONLY if segment info and segment tracks
  // elements were found as well.
  parse_status = segment_->ParseHeaders();
  if (parse_status) {
    LOG(INFO) << "segment header parse failed, parse_status=" << parse_status;
    return kNeedMoreData;
  }
  // Get the segment info to obtain its length.
  const mkvparser::SegmentInfo* ptr_segment_info = segment_->GetInfo();
  if (!ptr_segment_info) {
    LOG(ERROR) << "missing MKV segment info";
    return kParseError;
  }
  if (headers_length != ptr_segment_info->m_element_start) {
    LOG(ERROR) << "ERROR: unexpected segment info offset (expected="
               << headers_length << " actual="
               << ptr_segment_info->m_element_start << ")";
    return kParseError;
  }
  LOG(INFO) << "segment info size=" << ptr_segment_info->m_element_size;
  headers_length += ptr_segment_info->m_element_size;
  // Get the segment tracks to obtain its length.
  const mkvparser::Tracks* ptr_tracks = segment_->GetTracks();
  if (!ptr_tracks) {
    LOG(ERROR) << "missing MKV segment tracks";
    return kParseError;
  }
  LOG(INFO) << "segment tracks size=" << ptr_tracks->m_element_size;
  headers_length += ptr_tracks->m_element_size;
  LOG(INFO) << "element_size=" << headers_length;
  total_bytes_parsed_ = headers_length;
  *ptr_element_size = static_cast<int32>(headers_length);
  return kSuccess;
}

// Tries to parse a cluster.  Loads and parses the cluster header, and then
// attempts to walk through the cluster block entries.  Returns |kNeedMoreData|
// when unable to walk through all blocks in the cluster.  Returns |kSuccess|
// and sets |ptr_element_size| when successful.
int WebmBufferParser::ParseCluster(int32* ptr_element_size) {
  // A NULL |ptr_cluster_| means either:
  // - No clusters have been parsed, or...
  // - The last cluster was parsed.
  // In either case it's time to load a new one...
  int status;
  long length = 0;
  if (!ptr_cluster_) {
    // Load/parse a cluster header...
    int64 current_pos = total_bytes_parsed_;
    status = segment_->LoadCluster(current_pos, length);
    if (status) {
      return kNeedMoreData;
    }
    //DBGLOG("current_pos=" << current_pos << " length=" << length);
    const mkvparser::Cluster* ptr_cluster = segment_->GetLast();
    if (!ptr_cluster || ptr_cluster->EOS()) {
      return kNeedMoreData;
    }
    cluster_parse_offset_ = current_pos;
    ptr_cluster_ = ptr_cluster;
  }
  const int kClusterComplete = 1;
  for (;;) {
    status = ptr_cluster_->Parse(cluster_parse_offset_, length);
    //DBGLOG("cluster_parse_offset_=" << cluster_parse_offset_
    //       << " length=" << length);
    if (status == kClusterComplete) {
      break;
    }
    if (status < 0) {
      return kNeedMoreData;
    }
  }
  const int64 cluster_size = ptr_cluster_->GetElementSize();
  if (cluster_size == -1) {
    // Should never happen... the parser should never report a complete cluster
    // without setting its length.
    return kParseError;
  }
  ptr_cluster_ = NULL;
  cluster_parse_offset_ = 0;
  total_bytes_parsed_ += cluster_size;
  *ptr_element_size = static_cast<int32>(cluster_size);
  //DBGLOG("cluster_size=" << cluster_size << " total_bytes_parsed_="
  //       << total_bytes_parsed_);
  return kSuccess;
}

}  // namespace webmlive
