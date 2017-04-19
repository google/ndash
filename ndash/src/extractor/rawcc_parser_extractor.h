/*
 * Copyright 2017 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef NDASH_EXTRACTOR_STREAM_COPY_EXTRACTOR_H_
#define NDASH_EXTRACTOR_STREAM_COPY_EXTRACTOR_H_

#include <map>
#include <memory>

#include "base/time/time.h"
#include "extractor/extractor.h"
#include "util/time.h"

namespace ndash {
namespace extractor {

class TrackOutputInterface;

// Parses a RAWCC stream and produces samples for the dash player's
// sample queue.
//
// NOTE: RAWCC samples have very short durations (~1-2ms). Rather than
// producing one dashplayer sample for each RAWCC sample, we batch them
// together (up to kMaxEntriesPerSample at a time) with the dashplayer sample
// pts being the first pts encountered in the batch. The consumer of these
// samples must buffer and time the display of this data according to media
// time.
// The dashplayer's sample format is as follows:
//  repeat <= 120x {
//   pts (4 bytes in 45khz clock)
//   field/cc_type (1 byte)
//     0/1 = EIA-608. 0 = Top field, 1 = Bottom field.
//     2/3 = EIA-708, 2 = DTVCC_PACKET_DATA cc_type
//                    3 = DTVCC_PACKET_START cc_type
//   cc1 (1 byte)
//   cc2 (1 byte)
//   cc_valid (1 byte)
//  }
class RawCCParserExtractor : public ExtractorInterface {
 public:
  // Construct a RawCCParserExtractor
  // sample_offset : Offset used to translate PTS into the master timeline.
  // trunc_start_pts : The earliest PTS we should allow into the sample queue.
  // trunc_end_pts: The latest PTS we should allow into the sample queue.
  // NOTE: |trunc_start_pts| and |trunc_end_pts| should be media time PTS
  //       values, not master time-line values.
  RawCCParserExtractor(
      const base::TimeDelta sample_offset = base::TimeDelta(),
      std::unique_ptr<base::TimeDelta> trunc_start_pts = nullptr,
      std::unique_ptr<base::TimeDelta> trunc_end_pts = nullptr);
  ~RawCCParserExtractor() override;

  void Init(ExtractorOutputInterface* output) override;
  bool Sniff(ExtractorInputInterface* input) override;
  int Read(ExtractorInputInterface* input, int64_t* seek_position) override;
  void Seek() override;
  void Release() override;

 private:
  static constexpr size_t kReadBufferSize = 4096;
  static constexpr size_t kMaxEntriesPerSample = 120;

  enum ParsingState { PARSING_HEADER, PARSING_PTS_AND_COUNT, PARSING_ENTRIES };

  uint8_t ReadByte();
  uint32_t ReadInt();
  bool WriteFully(const char* src, size_t num);

  void FlushSample();
  void Reset();

  size_t available() const { return write_pos_ - read_pos_; }

  ParsingState state_;

  // Used to translate to master timeline.
  base::TimeDelta sample_offset_;
  // The earliest media PTS value that will be allowed into the sample queue.
  std::unique_ptr<base::TimeDelta> trunc_start_pts_;
  // The latest media PTS value that will be allowed into the sample queue.
  std::unique_ptr<base::TimeDelta> trunc_end_pts_;

  uint8_t buf_[kReadBufferSize];

  size_t write_pos_;
  size_t read_pos_;

  // Version parsed from the header.
  uint8_t version_;
  // pts from most recent rawcc sample (45khz clock).
  uint32_t pts_;
  // pts converted to microseconds for the sample queue.
  int64_t sample_pts_;
  // The number of samples expected from the rawcc stream.
  size_t expected_count_;
  // The current sample index within the rawcc stream.
  size_t sample_index_;
  // Total number of entries written to a sample before flushing.
  size_t total_written_;
  // Are we currently writing sample data to the queue?
  bool producing_to_queue_;

  ExtractorOutputInterface* output_;
  TrackOutputInterface* out_track_;
};

}  // namespace extractor
}  // namespace ndash

#endif  // NDASH_EXTRACTOR_STREAM_COPY_EXTRACTOR_H_
