/*
 * Copyright 2017 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "extractor/default_track_output.h"
#include "media_format.h"

namespace ndash {
namespace extractor {

constexpr int64_t DefaultTrackOutput::kInvalidTimestamp;

DefaultTrackOutput::DefaultTrackOutput(upstream::AllocatorInterface* allocator)
    : rolling_buffer_(allocator),
      sample_info_holder_(false),
      need_key_frame_(true),
      last_read_time_us_(kInvalidTimestamp),
      splice_out_time_us_(kInvalidTimestamp),
      largest_parsed_timestamp_us_(kInvalidTimestamp),
      format_(nullptr) {}

DefaultTrackOutput::~DefaultTrackOutput() {}

// Called by the consuming thread, but only when there is no loading thread.

void DefaultTrackOutput::Clear() {
  rolling_buffer_.Clear();
  need_key_frame_ = true;
  last_read_time_us_ = kInvalidTimestamp;
  splice_out_time_us_ = kInvalidTimestamp;
  largest_parsed_timestamp_us_ = kInvalidTimestamp;
}

int32_t DefaultTrackOutput::GetWriteIndex() const {
  return rolling_buffer_.GetWriteIndex();
}

void DefaultTrackOutput::DiscardUpstreamSamples(int32_t discard_from_index) {
  rolling_buffer_.DiscardUpstreamSamples(discard_from_index);
  largest_parsed_timestamp_us_ =
      rolling_buffer_.PeekSample(&sample_info_holder_)
          ? sample_info_holder_.GetTimeUs()
          : kInvalidTimestamp;
}

// Called by the consuming thread.

int32_t DefaultTrackOutput::GetReadIndex() const {
  return rolling_buffer_.GetReadIndex();
}

bool DefaultTrackOutput::HasFormat() const {
  return format_ != nullptr;
}

const MediaFormat* DefaultTrackOutput::GetFormat() const {
  return format_.get();
}

int64_t DefaultTrackOutput::GetLargestParsedTimestampUs() const {
  return largest_parsed_timestamp_us_;
}

bool DefaultTrackOutput::IsEmpty() {
  return !AdvanceToEligibleSample();
}

bool DefaultTrackOutput::GetSample(SampleHolder* holder) {
  DCHECK(holder != nullptr);
  bool found_eligible_sample = AdvanceToEligibleSample();
  if (!found_eligible_sample) {
    return false;
  }
  // Write the sample into the holder.
  if (rolling_buffer_.ReadSample(holder)) {
    // NOTE: Original code ignored return value from readSample.
    need_key_frame_ = false;
    last_read_time_us_ = holder->GetTimeUs();
    return true;
  }
  return false;
}

void DefaultTrackOutput::DiscardUntil(int64_t time_us) {
  while (rolling_buffer_.PeekSample(&sample_info_holder_) &&
         sample_info_holder_.GetTimeUs() < time_us) {
    rolling_buffer_.SkipSample();
    // We're discarding one or more samples. A subsequent read will need to
    // start at a keyframe.
    need_key_frame_ = true;
  }
  last_read_time_us_ = kInvalidTimestamp;
}

bool DefaultTrackOutput::SkipToKeyframeBefore(int64_t time_us) {
  return rolling_buffer_.SkipToKeyframeBefore(time_us);
}

bool DefaultTrackOutput::ConfigureSpliceTo(DefaultTrackOutput* next_queue) {
  if (splice_out_time_us_ != kInvalidTimestamp) {
    // We've already configured the splice.
    return true;
  }
  int64_t first_possible_splice_time;
  if (rolling_buffer_.PeekSample(&sample_info_holder_)) {
    first_possible_splice_time = sample_info_holder_.GetTimeUs();
  } else {
    first_possible_splice_time = last_read_time_us_ + 1;
  }
  RollingSampleBuffer* next_rolling_buffer = &next_queue->rolling_buffer_;
  while (next_rolling_buffer->PeekSample(&sample_info_holder_) &&
         (sample_info_holder_.GetTimeUs() < first_possible_splice_time ||
          !sample_info_holder_.IsSyncFrame())) {
    // Discard samples from the next queue for as long as they are before the
    // earliest possible
    // splice time, or not keyframes.
    next_rolling_buffer->SkipSample();
  }
  if (next_rolling_buffer->PeekSample(&sample_info_holder_)) {
    // We've found a keyframe in the next queue that can serve as the splice
    // point. Set the
    // splice point now.
    splice_out_time_us_ = sample_info_holder_.GetTimeUs();
    return true;
  }
  return false;
}

// Called by the loading thread.

// TrackOutput implementation. Called by the loading thread.

bool DefaultTrackOutput::WriteSampleData(ExtractorInputInterface* input,
                                         size_t length,
                                         bool allow_end_of_input,
                                         int64_t* bytes_appended) {
  LOG(FATAL) << "Unimplemented";
  return false;
}

void DefaultTrackOutput::WriteSampleData(const void* src, size_t length) {
  rolling_buffer_.AppendData(src, length);
}

bool DefaultTrackOutput::WriteSampleDataFixThis(const void* src,
                                                size_t length,
                                                bool allow_end_of_input,
                                                int64_t* num_bytes_written) {
  return rolling_buffer_.AppendDataFixThis(src, length, allow_end_of_input,
                                           num_bytes_written);
}

void DefaultTrackOutput::GiveFormat(std::unique_ptr<const MediaFormat> format) {
  format_ = std::move(format);
}

void DefaultTrackOutput::WriteSampleMetadata(
    int64_t time_us,
    int64_t duration_us,
    int32_t flags,
    size_t size,
    size_t offset,
    const std::string* encryption_key_id,
    const std::string* iv,
    std::vector<int32_t>* num_bytes_clear,
    std::vector<int32_t>* num_bytes_enc) {
  largest_parsed_timestamp_us_ =
      std::max(largest_parsed_timestamp_us_, time_us);
  rolling_buffer_.CommitSample(
      time_us, duration_us, flags,
      rolling_buffer_.GetWritePosition() - size - offset, size,
      encryption_key_id, iv, num_bytes_clear, num_bytes_enc);
}

// Private utility

bool DefaultTrackOutput::AdvanceToEligibleSample() {
  bool have_next = rolling_buffer_.PeekSample(&sample_info_holder_);
  if (need_key_frame_) {
    while (have_next && !sample_info_holder_.IsSyncFrame()) {
      rolling_buffer_.SkipSample();
      have_next = rolling_buffer_.PeekSample(&sample_info_holder_);
    }
  }
  if (!have_next) {
    return false;
  }
  if (splice_out_time_us_ != kInvalidTimestamp &&
      sample_info_holder_.GetTimeUs() >= splice_out_time_us_) {
    return false;
  }
  return true;
}

}  // namespace extractor
}  // namespace ndash
