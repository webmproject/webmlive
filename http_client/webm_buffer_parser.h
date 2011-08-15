// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef HTTP_CLIENT_WEBM_BUFFER_PARSER_H_
#define HTTP_CLIENT_WEBM_BUFFER_PARSER_H_

#include <string>
#include <vector>

#include "basictypes.h"
#include "boost/scoped_ptr.hpp"
#include "http_client_base.h"

namespace mkvparser {

class Cluster;
class Segment;

}  // namespace mkvparser

namespace webmlive {

class WebmBufferReader;
class WebmBufferParser {
 public:
  typedef std::vector<uint8> Buffer;
  enum {
    // Unexpected/unrecoverable parsing error.
    kParseError = -3,
    // Cannot allocate memory.
    kOutOfMemory = -2,
    // Invalid argument passed to method.
    kInvalidArg = -1,
    kSuccess = 0,
    // Parsing failed because data was exhausted before the end of an
    // element.  Add more data to your buffer.
    kNeedMoreData = 1,
  };
  enum ParseMode {
    // Parser is looking for segment info and segment tracks.
    kParseModeSegmentHeaders,
    // Parser is looking for clusters.
    kParseModeClusters,
  };
  WebmBufferParser();
  ~WebmBufferParser();
  // Constructs |reader_|.
  int Init();
  // Tries to parse some data in |buf| using |ParseSegmentHeaders| or
  // |ParseCluster|, depending on the |mode_| value.
  // Returns |kNeedMoreData| when more data is needed. Returns |kSuccess| and
  // sets |ptr_element_size| when all data has been parsed.
  int Parse(const Buffer& buf, int32* ptr_element_size);
 private:
  // Tries to parse the segment headers: segment info and segment tracks.
  // Returns |kNeedMoreData| if more data is needed.  Returns |kSuccess| and
  // sets |ptr_element_size| when successful.
  int ParseSegmentHeaders(int32* ptr_element_size);
  // Tries to parse a cluster.  Returns |kNeedMoreData| when more data is
  // needed. Returns |kSuccess| and sets |ptr_element_size| when all cluster
  // data has been parsed.
  int ParseCluster(int32* ptr_element_size);
  // Pointer to current cluster when |ParseCluster| only partially parses
  // cluster data.  NULL otherwise. Note that |ptr_cluster_| is memory owned by
  // libwebm's mkvparser.
  const mkvparser::Cluster* ptr_cluster_;
  // Pointer to libwebm segment; needed for cluster parsing operations.
  boost::scoped_ptr<mkvparser::Segment> segment_;
  // Buffer object that implements the IMkvReader interface required by
  // libwebm's mkvparser using a window into the |buf| argument passed to
  // |Parse|.
  boost::scoped_ptr<WebmBufferReader> reader_;
  // Bytes read in partially parsed cluster.
  int64 cluster_parse_offset_;
  // Sum of parsed element lengths.  Used to update |parser_| window.
  int64 total_bytes_parsed_;
  // Parsing mode-- either |kParseModeSegmentHeaders| or |kParseModeClusters|.
  ParseMode mode_;
  WEBMLIVE_DISALLOW_COPY_AND_ASSIGN(WebmBufferParser);
};

}  // namespace webmlive

#endif  // HTTP_CLIENT_WEBM_BUFFER_PARSER_H_
