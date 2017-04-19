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

#include <iomanip>
#include <iostream>

#ifndef NDASH_UTIL_HEX_H_
#define NDASH_UTIL_HEX_H_

// Helper class to make it easy to emit hex-formatted non-integer values.
// See Hex().
class HexValue {
 public:
  explicit HexValue(int value, int padding)
      : value_(value), padding_(padding) {}

 private:
  int value_;
  int padding_;

  friend std::ostream& operator<<(std::ostream& os, const HexValue& h) {
    return os << std::hex << std::uppercase << std::setfill('0')
              << std::setw(h.padding_) << static_cast<int>(h.value_);
  }
};

// Helper method to make it easy to emit hex-formatted non-integer values.
// The issue is that:
//   std::cout << std::hex() << (uint8_t)1;
// will emit '1' as a character rather than integer, and since it's a
// non-visible character then nothing will be displayed.
template <typename T>
HexValue Hex(T value) {
  return HexValue(static_cast<int>(value), sizeof(T) * 2);
}

#endif // NDASH_UTIL_HEX_H_
