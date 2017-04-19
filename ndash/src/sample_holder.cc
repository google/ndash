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

#include "sample_holder.h"

#include <cstdint>
#include <string>

#include "base/logging.h"
#include "crypto_info.h"

namespace ndash {

SampleHolder::SampleHolder(bool buffer_replacement_enabled)
    : buffer_len_(0),
      written_size_(0),
      peek_size_(0),
      flags_(0),
      time_us_(0),
      duration_us_(0),
      buffer_replacement_enabled_(buffer_replacement_enabled) {}

SampleHolder::~SampleHolder() {}

bool SampleHolder::EnsureSpaceForWrite(int length) {
  bool success;
  if (buffer_.get() == nullptr) {
    buffer_ = CreateReplacementBuffer(length, &success);
    buffer_len_ = length;
    return success;
  }

  // Check whether the current buffer is sufficient.
  int32_t capacity = buffer_len_ - written_size_;
  int32_t position = written_size_;
  int32_t required_capacity = position + length;
  if (capacity >= required_capacity) {
    return true;
  }

  // Instantiate a new buffer if possible.
  std::unique_ptr<uint8_t[]> new_buffer =
      CreateReplacementBuffer(required_capacity, &success);
  if (success) {
    // Copy data up to the current position from the old buffer to the new one.
    if (position > 0) {
      memcpy(new_buffer.get(), buffer_.get(), written_size_);
    }
    // Set the new buffer.
    buffer_ = std::move(new_buffer);
    buffer_len_ = required_capacity;
    return true;
  }
  return false;
}

void SampleHolder::SetDataBuffer(std::unique_ptr<uint8_t[]> data,
                                 int data_len) {
  buffer_ = std::move(data);
  buffer_len_ = data_len;
}

bool SampleHolder::Write(const uint8_t* data, int len) {
  int32_t capacity = buffer_len_ - written_size_;
  if (capacity >= len) {
    DCHECK(buffer_.get() != nullptr);
    memcpy(buffer_.get() + written_size_, data, len);
    written_size_ += len;
    return true;
  }
  return false;
}

void SampleHolder::SetPeekSize(int32_t size) {
  peek_size_ = size;
}

void SampleHolder::SetDurationUs(int64_t duration) {
  duration_us_ = duration;
}

void SampleHolder::SetFlags(int32_t flags) {
  flags_ = flags;
}

void SampleHolder::SetTimeUs(int64_t time_us) {
  time_us_ = time_us;
}

void SampleHolder::ClearData() {
  written_size_ = 0;
  buffer_len_ = 0;
  written_size_ = 0;
  peek_size_ = 0;
  flags_ = 0;
  time_us_ = 0;
  duration_us_ = 0;
  crypto_info_.MutableKey()->clear();
  crypto_info_.MutableIv()->clear();
  crypto_info_.MutableNumBytesClear()->clear();
  crypto_info_.MutableNumBytesEncrypted()->clear();
  crypto_info_.SetNumSubSamples(0);
}

std::unique_ptr<uint8_t[]> SampleHolder::CreateReplacementBuffer(
    int32_t required_capacity,
    bool* success) {
  if (buffer_replacement_enabled_) {
    // TODO(rmrossi): Consider switching our buffer to be vector of uint8_t and
    // resizing it instead of this new/copy approach.
    *success = true;
    return std::unique_ptr<uint8_t[]>(new uint8_t[required_capacity]());
  }
  *success = false;
  return nullptr;
}

}  // namespace ndash
