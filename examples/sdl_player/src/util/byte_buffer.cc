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

#include "util/byte_buffer.h"

#include <memory.h>
#include <cassert>

#include <glog/logging.h>

namespace util {

void PtsByteBuffer::Write(const size_t buffer_size,
                          const uint8_t* data,
                          const int64_t pts) {
  std::lock_guard<std::mutex> lock(byte_stream_lock_);
  byte_stream_.insert(byte_stream_.end(), data, data + buffer_size);
  pts_pos_.emplace_back(pos_pts{pts, buffer_size});
}

size_t PtsByteBuffer::Read(size_t requested_amount,
                           uint8_t* buffer,
                           int64_t* pts) {
  std::lock_guard<std::mutex> lock(byte_stream_lock_);
  if (pts_pos_.empty()) {
    return 0;
  }
  if (requested_amount > byte_stream_.size()) {
    requested_amount = byte_stream_.size();
  }
  if (requested_amount) {
    std::copy(byte_stream_.begin(), byte_stream_.begin() + requested_amount,
              buffer);
    byte_stream_.erase(byte_stream_.begin(),
                       byte_stream_.begin() + requested_amount);
  }
  size_t consumed_pts_bytes_amount = requested_amount;
  *pts = pts_pos_.front().pts;
  while (!pts_pos_.empty()) {
    size_t& front_avail_bytes = pts_pos_.front().available_bytes;
    if (front_avail_bytes <= consumed_pts_bytes_amount) {
      consumed_pts_bytes_amount -= front_avail_bytes;
      pts_pos_.pop_front();
    } else {
      front_avail_bytes -= consumed_pts_bytes_amount;
      break;
    }
  }
  return requested_amount;
}

size_t PtsByteBuffer::Available() const {
  std::lock_guard<std::mutex> lock(byte_stream_lock_);
  return byte_stream_.size();
}

void PtsByteBuffer::Flush() {
  std::lock_guard<std::mutex> lock(byte_stream_lock_);
  byte_stream_.clear();
  pts_pos_.clear();
}

size_t PtsByteBuffer::PtsDataAvailable() const {
  size_t total_avail(0);
  for (auto& pts_pos : pts_pos_) {
    total_avail += pts_pos.available_bytes;
  }
  return total_avail;
}

}  // namespace util
