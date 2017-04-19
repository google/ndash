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

#ifndef NDASH_EXTRACTOR_ROLLING_SAMPLE_BUFFER_H_
#define NDASH_EXTRACTOR_ROLLING_SAMPLE_BUFFER_H_

#include <cstdint>
#include <deque>

#include "base/synchronization/lock.h"
#include "extractor/info_queue.h"
#include "upstream/allocator.h"
#include "upstream/data_source.h"

namespace ndash {

namespace extractor {

// A rolling buffer of sample data and corresponding sample information.
class RollingSampleBuffer {
 public:
  explicit RollingSampleBuffer(upstream::AllocatorInterface* allocator);
  ~RollingSampleBuffer();

  // Called by the consuming thread, but only when there is no loading thread.

  // Clears the buffer, returning all allocations to the allocator.
  void Clear();

  // Returns the current absolute write index.
  int32_t GetWriteIndex() const;

  // Discards samples from the write side of the buffer.
  // discard_from_index: The absolute index of the first sample to be discarded.
  void DiscardUpstreamSamples(int32_t discard_from_index);

  // Called by the consuming thread.

  // Returns the current absolute read index.
  int32_t GetReadIndex() const;

  // Fills the holder with information about the current sample, but does not
  // write its data.
  // holder: The holder into which the current sample information should be
  //         written.
  // Returns true if the holder was filled, false if there is no current sample.
  bool PeekSample(SampleHolder* sample_holder);

  // Skips the current sample.
  void SkipSample();

  // Attempts to skip to the keyframe before the specified time, if it's
  // present in the buffer.
  // time_us: The seek time.
  // Returns true if the skip was successful. False otherwise.
  bool SkipToKeyframeBefore(int64_t time_us);

  // Reads the current sample, advancing the read index to the next sample.
  // sample_holder: The holder into which the current sample should be written.
  // Returns true if a sample was read, false if there is no current sample.
  bool ReadSample(SampleHolder* sample_holder);

  // Called by the loading thread.

  // Returns the current write position in the rolling buffer.
  // @return The current write position.
  int64_t GetWritePosition() const;

  // Appends data to the rolling buffer.
  // data_source: The source from which to read.
  // length: The maximum length of the read.
  // allow_end_of_input: True if encountering the end of the input having
  //                     appended no data is allowed, and should result in
  //                     kResultEndOfInput being returned. False if it should
  //                     be considered an error.
  // Returns the number of bytes appended, or kResultEndOfInput if the input has
  // ended.
  // @throws IOException If an error occurs reading from the source.
  bool AppendData(upstream::DataSourceInterface* data_source,
                  size_t length,
                  bool allow_end_of_input,
                  int64_t* num_bytes_appended);

  // Appends data to the rolling buffer.
  // input: The source from which to read.
  // length: The maximum length of the read.
  // allow_end_of_input: True if encountering the end of the input having
  //                     appended no data is allowed, and should result in
  //                     kResultEndOfInput being returned. False if it should
  //                     be considered an error.
  // Returns the number of bytes appended, or kResultEndOfInput if
  // the input has ended.
  int32_t AppendData(upstream::DataSourceInterface* data_source,
                     bool allow_end_of_input);
  void AppendData(const void* src, size_t length);

  // TODO(rmrossi): Replace the above function with this version that indicates
  // whether all data was appended.
  bool AppendDataFixThis(const void* src,
                         size_t length,
                         bool allow_end_of_input,
                         int64_t* num_bytes_appended);

  // Indicates the end point for the current sample, making it available for
  // consumption.
  // sample_time_us: The sample timestamp.
  // duration_us: The sample duration in microseconds.
  // flags: Flags that accompany the sample. See SampleHolder::GetFlags().
  // position: The position of the sample data in the rolling buffer.
  // size: The size of the sample, in bytes.
  // encryption_key_id: The encryption key associated with the sample, or null.
  // iv: The initialization vector if it is encrypted, or null.
  // num_bytes_clear: The clear byte counts if the sample is encrypted, or null.
  // num_bytes_enc: The encrypted byte counts, or null.
  // Note: Encrypted data may contain clear (unencrypted) and encrypted regions.
  // The num_bytes_clear and num_bytes_enc arrays describe the number of bytes
  // of each type in each back-to-back region of the data starting at pos 0.
  // TODO(rmrossi): Use a vector of structs instead of two vectors like
  // MP4 parser does.  Then each i'th element describes one region more clearly.
  void CommitSample(int64_t sample_time_us,
                    int64_t duration_us,
                    int32_t flags,
                    int64_t position,
                    int32_t size,
                    const std::string* encryption_key_id = nullptr,
                    const std::string* iv = nullptr,
                    std::vector<int32_t>* num_bytes_clear = nullptr,
                    std::vector<int32_t>* num_bytes_enc = nullptr);

 private:
  RollingSampleBuffer(const RollingSampleBuffer& other) = delete;

  void ReadDataToScratch(int64_t absolute_position, int32_t length);
  int32_t ReadUnsignedShortFromScratch();
  int32_t ReadUnsignedIntToIntFromScratch();

  // Ensure that the scratch vector is of at least the specified capacity.
  void EnsureScratchCapacity(int32_t capacity);

  // Reads data from the front of the rolling buffer.
  // absolute_position: The absolute position from which data should be read.
  //                    target The array into which data should be written.
  // dest: The destination char array pointer.
  // length: The number of bytes to read.
  void ReadData(int64_t absolute_position, char* dest, int32_t length);

  void ReadDataToSampleHolder(int64_t absolute_position,
                              SampleHolder* dest,
                              int32_t length);

  // Reads encryption data for the current sample.
  // The encryption data is written into the holder. The holder's
  // size: is adjusted to subtract the number of bytes that were read. The
  //       same value is added to the holder's offset.
  // sample_holder: The holder into which the encryption data should be
  //                written.
  // extras_holder: The extras holder whose offset should be read and
  //                subsequently adjusted.
  void ReadEncryptionData(SampleHolder* holder,
                          SampleExtrasHolder* extras_holder);

  // Discards data from the write side of the buffer. Data is discarded from
  // the specified absolute position. Any allocations that are fully discarded
  // are returned to the allocator.
  // absolute_position: The absolute position (inclusive) from which to
  //                    discard data.
  void DropUpstreamFrom(int64_t absolute_position);

  // Discard any allocations that hold data prior to the specified absolute
  // position, returning them to the allocator.
  // absolute_position: The absolute position up to which allocations can
  //                    be discarded.
  void DropDownstreamTo(int64_t absolute_position);

  // Prepares the rolling sample buffer for an append of up to length bytes,
  // returning the number of bytes that can actually be appended.
  int32_t PrepareForAppend(int32_t length);

  mutable base::Lock lock_;

  upstream::AllocatorInterface* allocator_;
  int32_t allocation_length_;

  InfoQueue info_queue_;
  std::deque<const upstream::AllocatorInterface::AllocationType*> data_queue_;
  SampleExtrasHolder extras_holder_;
  std::vector<char> scratch_;
  int32_t scratch_position_;

  // Accessed only by the consuming thread.
  int64_t total_bytes_dropped_;

  // Accessed only by the loading thread.
  int64_t total_bytes_written_;
  upstream::AllocatorInterface::AllocationType* last_allocation_;
  int32_t last_allocation_offset_;
};

}  // namespace extractor

}  // namespace ndash

#endif  // NDASH_EXTRACTOR_ROLLING_SAMPLE_BUFFER_H_
