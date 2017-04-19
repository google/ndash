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

#ifndef NDASH_EXTRACTOR_INFO_QUEUE_H_
#define NDASH_EXTRACTOR_INFO_QUEUE_H_

#include <algorithm>
#include <memory>
#include <string>

#include "base/logging.h"
#include "base/synchronization/lock.h"
#include "sample_holder.h"

namespace ndash {

namespace extractor {

constexpr int32_t kSampleCapacityIncrement = 1000;

class SampleExtrasHolder {
 public:
  SampleExtrasHolder();
  ~SampleExtrasHolder();

  int32_t GetOffset() const;
  void SetOffset(int32_t offset);

  const std::string* GetEncryptionKeyId() const;
  void SetEncryptionKeyId(const std::string* encryption_key_id);

  const std::string* GetIV() const;
  void SetIV(const std::string* ivs);

  const std::vector<int32_t>* GetNumBytesClear() const;
  void SetNumBytesClear(const std::vector<int32_t>* num_bytes_clear);

  const std::vector<int32_t>* GetNumBytesEnc() const;
  void SetNumBytesEnc(const std::vector<int32_t>* num_bytes_enc);

 private:
  int64_t offset_;

  // The encryption key id if this sample is encrypted, or null.
  // Storage is owned by the InfoQueue.
  const std::string* encryption_key_id_;

  // The initialization vector if this sample is encrypted, or null.
  // Storage is owned by the InfoQueue.
  const std::string* iv_;

  // Pointers to clear byte counts.
  // Storage is owned by the info queue.
  const std::vector<int32_t>* num_bytes_clear_;

  // Pointers to encrypted byte counts.
  // Storage is owned by the info queue.
  const std::vector<int32_t>* num_bytes_enc_;
};

// TODO (rmrossi): Replace the array storage + 'auto-expanding' behavior
// with a std::deque and define a struct to hold the meta-data elements. This
// will simplify the implementation quite a bit.

// Holds information about the samples in the rolling buffer.
class InfoQueue {
 public:
  InfoQueue();
  ~InfoQueue();

  // Called by the consuming thread, but only when there is no loading thread.

  // Clears the queue.
  void Clear();

  // Returns the current absolute write index.
  int32_t GetWriteIndex() const;

  // Discards samples from the write side of the buffer.
  // discardFromIndex The absolute index of the first sample to be discarded.
  // Returns the reduced total number of bytes written, after the samples have
  // been discarded.
  int64_t DiscardUpstreamSamples(int32_t discard_from_index);

  // Called by the consuming thread.

  // Returns the current absolute read index.
  int32_t GetReadIndex() const;

  // Fills SampleHolder with information about the current sample, but does not
  // write its data. The first entry in offsetHolder is filled with the
  // absolute position of the sample's data in the rolling buffer.
  // Populates SampleHolder size, time_us and flags
  //
  // holder The holder into which the current sample information should be
  // written.
  // returns true if the holders were filled, false if there is no current
  // sample.
  bool PeekSample(SampleHolder* holder, SampleExtrasHolder* extras_holder);

  // Advances the read index to the next sample.
  // Returns the absolute position of the first byte in the rolling buffer that
  // may still be required after advancing the index. Data prior to this
  // position can be dropped.
  int64_t MoveToNextSample();

  // Attempts to locate the keyframe before the specified time, if it's present
  // in the buffer.
  // timeUs The seek time.
  // Returns the offset of the keyframe's data if the keyframe was present.
  // -1 otherwise.
  int64_t SkipToKeyframeBefore(int64_t time_us);

  // Called by the loading thread.
  void CommitSample(int64_t time_us,
                    int64_t duration_us,
                    int32_t sample_flags,
                    int64_t offset,
                    int32_t size,
                    const std::string* encryption_key_id = nullptr,
                    const std::string* iv = nullptr,
                    std::vector<int32_t>* num_bytes_clear = nullptr,
                    std::vector<int32_t>* num_bytes_enc = nullptr);

 private:
  int32_t GetWriteIndexInternal() const;

  void CopyStrings(std::string* dest,
                   int32_t dest_pos,
                   std::string* src,
                   int32_t src_pos,
                   int len);

  void CopyIntVectors(std::vector<int32_t>* dest,
                      int32_t dest_pos,
                      std::vector<int32_t>* src,
                      int32_t src_pos,
                      int32_t len);

  mutable base::Lock lock_;

  int32_t capacity_;

  std::unique_ptr<int64_t[]> offsets_;
  std::unique_ptr<int64_t[]> durations_;
  std::unique_ptr<int32_t[]> sizes_;
  std::unique_ptr<int32_t[]> flags_;
  std::unique_ptr<int64_t[]> times_us_;
  std::unique_ptr<std::string[]> encryption_key_ids_;
  std::unique_ptr<std::string[]> iv_;
  std::unique_ptr<std::vector<int32_t>[]> num_bytes_clear_;
  std::unique_ptr<std::vector<int32_t>[]> num_bytes_enc_;

  int32_t queue_size_;
  int32_t absolute_read_index_;
  int32_t relative_read_index_;
  int32_t relative_write_index_;
};

}  // namespace extractor

}  // namespace ndash

#endif  // NDASH_EXTRACTOR_INFO_QUEUE_H_
