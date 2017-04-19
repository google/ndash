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

#include "extractor/rawcc_parser_extractor.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "extractor/chunk_index.h"
#include "extractor/extractor_input.h"
#include "extractor/extractor_output.h"
#include "extractor/track_output.h"
#include "media_format.h"
#include "util/mime_types.h"
#include "util/util.h"

namespace ndash {
namespace extractor {

namespace {
constexpr size_t kRawCCHeaderSize = 8;
constexpr uint32_t kRawCCHeader = 'R' << 24 | 'C' << 16 | 'C' << 8 | 0x01;
constexpr size_t kRawCCFlagsSize = 3;

constexpr size_t kRawCCPtsAndCountSizeV0 = 5;
constexpr size_t kRawCCSampleSize = 3;

constexpr size_t kSampleEntrySize = 8;

}  // namespace

RawCCParserExtractor::RawCCParserExtractor(
    const base::TimeDelta sample_offset,
    std::unique_ptr<base::TimeDelta> trunc_start_pts,
    std::unique_ptr<base::TimeDelta> trunc_end_pts)
    : sample_offset_(sample_offset),
      trunc_start_pts_(std::move(trunc_start_pts)),
      trunc_end_pts_(std::move(trunc_end_pts)),
      output_(nullptr),
      out_track_(nullptr) {
  Reset();
}

RawCCParserExtractor::~RawCCParserExtractor() {}

void RawCCParserExtractor::Init(ExtractorOutputInterface* output) {
  output_ = output;
  out_track_ = output_->RegisterTrack(0);
}

bool RawCCParserExtractor::Sniff(ExtractorInputInterface* input) {
  // We aren't implementing the ExtractorInput buffering required to make this
  // work, so just be optimistic and always return true.
  return true;
}

uint8_t RawCCParserExtractor::ReadByte() {
  uint8_t v = buf_[read_pos_++];
  return v;
}

uint32_t RawCCParserExtractor::ReadInt() {
  // network byte order
  uint32_t v = buf_[read_pos_++] << 24;
  v |= buf_[read_pos_++] << 16;
  v |= buf_[read_pos_++] << 8;
  v |= buf_[read_pos_++];
  return v;
}

int RawCCParserExtractor::Read(ExtractorInputInterface* input,
                               int64_t* seek_position) {
  int n = available();
  if (n > 0 && read_pos_ != 0) {
    // Move any unconsumed data back to the beginning of our buf.
    memmove(buf_, buf_ + read_pos_, n);
    read_pos_ = 0;
    write_pos_ = n;
  } else if (n == 0) {
    read_pos_ = 0;
    write_pos_ = 0;
  }

  ssize_t result = input->Read(buf_ + write_pos_, kReadBufferSize - write_pos_);

  if (result == 0)
    return RESULT_CONTINUE;

  if (result == RESULT_END_OF_INPUT) {
    Reset();
    VLOG(3) << "End of input";
    return RESULT_END_OF_INPUT;
  }

  if (result < 0) {
    return RESULT_IO_ERROR;
  }

  write_pos_ += result;

  while (read_pos_ < write_pos_) {
    base::TimeDelta this_sample_pts;
    switch (state_) {
      case PARSING_HEADER:
        if ((available()) < kRawCCHeaderSize) {
          // Waiting for more data.
          VLOG(3) << "PARSING_HEADER: waiting for more data";
          return RESULT_CONTINUE;
        } else {
          size_t read_start = read_pos_;
          uint32_t hdr = ReadInt();
          if (hdr != kRawCCHeader) {
            LOG(ERROR) << "Invalid RAWCC header";
            return RESULT_IO_ERROR;
          }

          version_ = ReadByte();
          // Grab pts and count from cc packet.
          if (version_ != 0x00) {
            // TODO(rmrossi): Support version 1 with 8 byte pts.
            LOG(ERROR) << "Unsupported rawcc version";
            return RESULT_IO_ERROR;
          }

          // Skip over flags. None are used yet.
          read_pos_ += kRawCCFlagsSize;
          // Fall through to the next state.
          VLOG(3) << "PARSING_HEADER -> PARSING_PTS_AND_COUNT";
          state_ = PARSING_PTS_AND_COUNT;

          size_t read_end = read_pos_;
          DCHECK_EQ(read_end - read_start, kRawCCHeaderSize);
        }
      // No break
      case PARSING_PTS_AND_COUNT:
        if (available() < kRawCCPtsAndCountSizeV0) {
          // Waiting for more data.
          VLOG(3) << "PARSING_PTS_AND_COUNT: waiting for more data";
          return RESULT_CONTINUE;
        }

        // 45khz
        pts_ = ReadInt();

        this_sample_pts = base::TimeDelta::FromMicroseconds(
            util::Util::ScaleLargeTimestamp(pts_, util::kMicrosPerMs, 45));

        producing_to_queue_ = true;
        // Only produce to the queue what falls between the given range
        // (if given)
        if (trunc_start_pts_ && this_sample_pts < *trunc_start_pts_) {
          // Falls outside truncation window. Ignore it.
          producing_to_queue_ = false;
        } else if (trunc_end_pts_.get() && this_sample_pts > *trunc_end_pts_) {
          // Falls outside truncation window. Ignore it.
          producing_to_queue_ = false;
        }

        if (total_written_ == 0) {
          // Use first pts encountered as the sample pts.
          sample_pts_ = this_sample_pts.InMicroseconds();
        }

        expected_count_ = ReadByte();

        sample_index_ = 0;
        // Fall through to the next state.
        VLOG(3) << "PARSING_PTS_AND_COUNT -> PARSING_ENTRIES";
        state_ = PARSING_ENTRIES;
      // No break
      case PARSING_ENTRIES:
        // Translate |expected_count_| entries.
        while (sample_index_ < expected_count_) {
          if (available() < kRawCCSampleSize) {
            // Need more bytes.
            VLOG(3) << "PARSING_ENTRIES: waiting for more data";
            return RESULT_CONTINUE;
          }

          uint8_t flags = ReadByte();
          uint8_t cc1 = ReadByte();
          uint8_t cc2 = ReadByte();

          if (!producing_to_queue_) {
            // We are not within range yet.
            sample_index_++;
            continue;
          }

          // Translate rawcc format into UDP format.
          char entry[kSampleEntrySize];
          size_t pos = 0;
          uint32_t entry_pts = pts_;

          if (sample_offset_ != base::TimeDelta()) {
            // Convert from 45khz to time
            base::TimeDelta sample_pts = base::TimeDelta::FromMicroseconds(
                util::Util::ScaleLargeTimestamp(pts_, util::kMicrosPerMs, 45));
            // This brings us to how far into the stream this sample is
            // relative to the beginning.
            sample_pts += sample_offset_;
            // Back to 45khz
            entry_pts = sample_pts.InMilliseconds() * 45;
          }

          // Back to 45khz
          // network byte order
          entry[pos++] = (entry_pts >> 24) & 0xFF;
          entry[pos++] = (entry_pts >> 16) & 0xFF;
          entry[pos++] = (entry_pts >> 8) & 0xFF;
          entry[pos++] = entry_pts & 0xFF;
          // field
          entry[pos++] = (flags & 0x03);
          // cc1
          entry[pos++] = cc1;
          // cc2
          entry[pos++] = cc2;
          // cc_valid
          entry[pos++] = (flags & 0x04) != 0;

          DCHECK_EQ(pos, kSampleEntrySize);
          // Write the sample.
          if (!WriteFully(&entry[0], kSampleEntrySize)) {
            return RESULT_IO_ERROR;
          }
          sample_index_++;
          total_written_++;
          if (total_written_ > kMaxEntriesPerSample) {
            FlushSample();
          }
        }

        // Reset for next iteration.
        expected_count_ = 0;
        sample_index_ = 0;
        VLOG(3) << "PARSING_ENTRIES -> PARSING_PTS_AND_COUNT";
        state_ = PARSING_PTS_AND_COUNT;
        // No break
    }
  }

  FlushSample();
  return RESULT_CONTINUE;
}

void RawCCParserExtractor::FlushSample() {
  if (total_written_ > 0 && producing_to_queue_) {
    int64_t last_pts =
        util::Util::ScaleLargeTimestamp(pts_, util::kMicrosPerMs, 45);

    out_track_->WriteSampleMetadata(sample_pts_, last_pts - sample_pts_,
                                    util::kSampleFlagSync,
                                    total_written_ * kSampleEntrySize, 0);
    total_written_ = 0;
  }
}

bool RawCCParserExtractor::WriteFully(const char* src, size_t num) {
  size_t remain = num;
  size_t pos = 0;
  while (remain > 0) {
    int64_t num_appended;
    if (!out_track_->WriteSampleDataFixThis(src + pos, remain, true,
                                            &num_appended)) {
      return false;
    }
    remain -= num_appended;
    pos += num_appended;
  }
  return true;
}

void RawCCParserExtractor::Seek() {
  Reset();
}

void RawCCParserExtractor::Release() {
  output_ = nullptr;
}

void RawCCParserExtractor::Reset() {
  total_written_ = 0;
  expected_count_ = 0;
  read_pos_ = 0;
  write_pos_ = 0;
  sample_index_ = 0;
  producing_to_queue_ = true;
  sample_pts_ = 0;
  pts_ = 0;
  version_ = 0;
  state_ = PARSING_HEADER;
}

}  // namespace extractor
}  // namespace ndash
