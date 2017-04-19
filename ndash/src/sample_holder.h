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

#ifndef NDASH_SAMPLE_HOLDER_H_
#define NDASH_SAMPLE_HOLDER_H_

#include <cstdint>
#include <string>

#include "crypto_info.h"
#include "util/util.h"

namespace ndash {

class SampleHolder {
 public:
  // buffer_replacement_enabled determines the behavior of
  // EnsureSpaceForWrite(int). See below for more details.
  explicit SampleHolder(bool buffer_replacement_enabled);
  ~SampleHolder();

  // Ensures that the buffer is large enough to accommodate a write of a
  // given length at its current position.
  // If the capacity of data is sufficient this method does nothing. If the
  // capacity is insufficient then an attempt is made to replace data with a
  // new buffer whose capacity is sufficient. Data up to the current position
  // is copied to the new buffer.
  // length The length of the write that must be accommodated, in bytes.
  // returns false If there is insufficient capacity to accommodate the write
  // and buffer_replacement_enabled is false.
  bool EnsureSpaceForWrite(int length);

  CryptoInfo* MutableCryptoInfo() { return &crypto_info_; }
  const CryptoInfo* GetCryptoInfo() const { return &crypto_info_; }

  void SetDataBuffer(std::unique_ptr<uint8_t[]> data, int data_len);

  bool Write(const uint8_t* data, int len);

  const uint8_t* GetBuffer() const { return buffer_.get(); }

  int32_t GetWrittenSize() const { return written_size_; }
  int32_t GetPeekSize() const { return peek_size_; }
  void SetPeekSize(int32_t size);
  int64_t GetDurationUs() const { return duration_us_; }
  void SetDurationUs(int64_t duration_ms);
  int32_t GetFlags() const { return flags_; }
  void SetFlags(int32_t flags);
  int64_t GetTimeUs() const { return time_us_; }
  void SetTimeUs(int64_t time_us);

  // Returns whether flags has kSampleFlagEncrypted set.
  bool IsEncrypted() const {
    return (flags_ & util::kSampleFlagEncrypted) != 0;
  }
  // Returns whether flags has kSampleFlagDecodeOnly set.
  bool IsDecodeOnly() const {
    return (flags_ & util::kSampleFlagDecodeOnly) != 0;
  }
  // Returns whether flags has kSampleFlagSync set.
  bool IsSyncFrame() const { return (flags_ & util::kSampleFlagSync) != 0; }
  // Sets the write position back to the beginning of the buffer.
  void ClearData();

 private:
  CryptoInfo crypto_info_;

  // A buffer holding the sample data.
  std::unique_ptr<uint8_t[]> buffer_;

  // The size of our data buffer.
  int32_t buffer_len_;

  // The number of bytes written into this sample holder.  Also represents the
  // next write position if this sample holder is being written to.
  int32_t written_size_;

  // Holds the size of a sample when this holder is used in a peek.
  int32_t peek_size_;

  // Flags that accompany the sample. A combination of kSampleFlagSync,
  // kSampleFlagEncrypted and kSampleFlagDecodeOnly.
  int32_t flags_;

  // The time at which the sample should be presented.
  int64_t time_us_;

  // Duration of this sample in microseconds.
  int64_t duration_us_;

  bool buffer_replacement_enabled_;

  std::unique_ptr<uint8_t[]> CreateReplacementBuffer(int32_t required_capacity,
                                                     bool* success);
};

}  // namespace ndash

#endif  // NDASH_SAMPLE_HOLDER_H_
