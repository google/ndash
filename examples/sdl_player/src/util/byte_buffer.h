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

#ifndef UTIL_BYTE_BUFFER_H_
#define UTIL_BYTE_BUFFER_H_

#include <cstdint>
#include <deque>
#include <mutex>

namespace util {

// A simple byte buffer which keeps the data portions in it in sync with the PTS
// of the data they were originally associated with.
class PtsByteBuffer {
 public:
  // Add data of 'buffer_size' to the buffer, and associate it with 'pts'.
  void Write(const size_t buffer_size, const uint8_t* data, const int64_t pts);

  // Remove data up to 'requested_amount' from the buffer if it is available,
  // and copy it into 'buffer'.
  // The first pts found in the associated data is set in 'pts'.
  // Returns the amount of data put into the buffer.
  size_t Read(size_t requested_amount, uint8_t* buffer, int64_t* pts);

  // Return the number of bytes available in the buffer.
  size_t Available() const;

  // Flushes all data and PTS values from the buffer.
  void Flush();

  // Return the number of bytes available according to the pts_pos_ queue.
  size_t PtsDataAvailable() const;

 private:
  struct pos_pts {
    int64_t pts;
    size_t available_bytes;
  };

  mutable std::mutex byte_stream_lock_;
  std::deque<uint8_t> byte_stream_;
  std::deque<pos_pts> pts_pos_;
};
}

#endif  // UTIL_BYTE_BUFFER_H_
