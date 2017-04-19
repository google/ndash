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

#ifndef NDASH_EXTRACTOR_DEFAULT_TRACK_OUTPUT_H_
#define NDASH_EXTRACTOR_DEFAULT_TRACK_OUTPUT_H_

#include <cstdint>
#include <cstdlib>
#include <limits>
#include <memory>

#include "extractor/indexed_track_output.h"
#include "extractor/rolling_sample_buffer.h"
#include "upstream/allocator.h"

namespace ndash {

class MediaFormat;
class SampleHolder;

namespace extractor {

// Buffers extracted samples in a queue, and allows for consumption from
// that queue.
class DefaultTrackOutput : public IndexedTrackOutputInterface {
 public:
  static constexpr int64_t kInvalidTimestamp =
      std::numeric_limits<int64_t>::min();

  explicit DefaultTrackOutput(upstream::AllocatorInterface* allocator);
  ~DefaultTrackOutput() override;

  // Called by the consuming thread, but only when there is no loading thread.

  // Clears the queue, returning all allocations to the allocator.
  void Clear();

  // Returns the current absolute write index.
  int32_t GetWriteIndex() const override;

  // Discards samples from the write side of the queue.
  // discard_from_index The absolute index of the first sample to be discarded.
  void DiscardUpstreamSamples(int32_t discard_from_index);

  // Called by the consuming thread.

  // Returns the current absolute read index.
  int32_t GetReadIndex() const;

  // Returns true if the output has received a format. False otherwise.
  bool HasFormat() const;

  // The format most recently received by the output, or null if a format has
  // yet to be received.
  const MediaFormat* GetFormat() const;

  // The largest timestamp of any sample received by the output, or LLONG_MIN
  // if a sample has yet to be received.
  int64_t GetLargestParsedTimestampUs() const;

  // Returns true if at least one sample can be read from the queue. False
  // otherwise.
  bool IsEmpty();

  // Removes the next sample from the head of the queue, writing it into the
  // provided holder.
  // The first sample returned is guaranteed to be a keyframe, since any
  // non-keyframe samples queued prior to the first keyframe are discarded.
  //
  // holder: A SampleHolder into which the sample should be read. Returns true
  //         if a sample was read. False otherwise.
  bool GetSample(SampleHolder* holder);

  // Discards samples from the queue up to the specified time.
  // time_us: The time up to which samples should be discarded, in microseconds.
  void DiscardUntil(int64_t time_us);

  // Attempts to skip to the keyframe before the specified time, if it's
  // present in the buffer.
  // time_us: The seek time.
  // Returns true if the skip was successful. False otherwise.
  bool SkipToKeyframeBefore(int64_t time_us);

  // Attempts to configure a splice from this queue to the next.
  // next_queue: The queue being spliced to.
  // Returns whether the splice was configured successfully.
  bool ConfigureSpliceTo(DefaultTrackOutput* next_queue);

  // Called by the loading thread.

  // TrackOutput implementation. Called by the loading thread.
  bool WriteSampleData(ExtractorInputInterface* input,
                       size_t length,
                       bool allow_end_of_input,
                       int64_t* bytes_appended) override;
  void WriteSampleData(const void* src, size_t length) override;
  bool WriteSampleDataFixThis(const void* src,
                              size_t length,
                              bool allow_end_of_input,
                              int64_t* num_bytes_written) override;

  void GiveFormat(std::unique_ptr<const MediaFormat> format) override;
  void WriteSampleMetadata(
      int64_t time_us,
      int64_t duration_us,
      int32_t flags,
      size_t size,
      size_t offset,
      const std::string* encryption_key_id = nullptr,
      const std::string* iv = nullptr,
      std::vector<int32_t>* num_bytes_clear = nullptr,
      std::vector<int32_t>* num_bytes_enc = nullptr) override;

 private:
  // Advances the underlying buffer to the next sample that is eligible to be
  // returned.
  // returns true if an eligible sample was found. False otherwise, in which
  // case the underlying buffer has been emptied.
  bool AdvanceToEligibleSample();

  RollingSampleBuffer rolling_buffer_;
  SampleHolder sample_info_holder_;

  // Accessed only by the consuming thread.
  bool need_key_frame_;
  int64_t last_read_time_us_;
  int64_t splice_out_time_us_;

  // Accessed by both the loading and consuming threads.
  int64_t largest_parsed_timestamp_us_;
  std::unique_ptr<const MediaFormat> format_;
};

}  // namespace extractor
}  // namespace ndash

#endif  // NDASH_EXTRACTOR_DEFAULT_TRACK_OUTPUT_H_
