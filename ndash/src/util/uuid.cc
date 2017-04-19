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

#include "util/uuid.h"

#include <endian.h>

#include <cctype>
#include <cstring>
#include <iomanip>

#include "util/hex.h"

namespace ndash {

namespace util {

constexpr int kValidStringLength = 32 + 4;  // 32 hex digits + 4 '-'s.

// Example UUID: 09514A5C-F8EB-4B5F-B0C3-97F52B47AE8A
Uuid Uuid::Parse(std::string string) {
  if (string.length() != kValidStringLength) {
    // Not valid in any form.
    return Uuid();
  }
  Uuid result;
  int b = 0;
  int i = 0;
  while ((i < kValidStringLength - 1) && (b < 16)) {
    uint8_t byte = 0;
    char c1 = std::tolower(string[i]);
    if (c1 == '-') {
      ++i;
      continue;
    } else if (c1 == 0) {
      break;
    } else if (c1 >= '0' && c1 <= '9') {
      byte = (c1 - '0') << 4;
    } else if (c1 >= 'a' && c1 <= 'f') {
      byte = (c1 - 'a' + 10) << 4;
    } else {
      return Uuid();
    }
    char c2 = std::tolower(string.data()[i + 1]);
    if (c2 == '-' || c2 == 0) {
      return Uuid();
    } else if (c2 >= '0' && c2 <= '9') {
      byte |= c2 - '0';
    } else if (c2 >= 'a' && c2 <= 'f') {
      byte |= c2 - 'a' + 10;
    } else {
      return Uuid();
    }
    result.value[b++] = byte;
    i += 2;
  }
  if (b != 16) {
    // Overflow/underflow.
    return Uuid();
  }
  return result;
}

void Uuid::ToString(std::string* output) const {
  output->clear();
  char buf[kValidStringLength + 1];
  snprintf(
      buf, kValidStringLength + 1,
      "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
      value[0], value[1], value[2], value[3], value[4], value[5], value[6],
      value[7], value[8], value[9], value[10], value[11], value[12], value[13],
      value[14], value[15]);
  output->append(std::string(buf));
}

namespace {

inline uint64_t PackBytes(const uint8_t* bytes) {
  uint64_t packed = *reinterpret_cast<const uint64_t*>(bytes);
  packed = be64toh(packed);
  return packed;
}

inline void UnpackBytes(uint64_t packed, uint8_t* bytes) {
  packed = be64toh(packed);
  *reinterpret_cast<uint64_t*>(bytes) = packed;
}

}  // namespace

uint64_t Uuid::most_significant_bits() const {
  return PackBytes(value);
}

void Uuid::set_most_significant_bits(uint64_t most_significant_bits) {
  UnpackBytes(most_significant_bits, value);
}

uint64_t Uuid::least_significant_bits() const {
  return PackBytes(&value[8]);
}

void Uuid::set_least_significant_bits(uint64_t least_significant_bits) {
  UnpackBytes(least_significant_bits, &value[8]);
}

std::ostream& operator<<(std::ostream& os, const Uuid& uuid) {
  return os << Hex(uuid.value[0]) << Hex(uuid.value[1]) << Hex(uuid.value[2])
            << Hex(uuid.value[3]) << "-" << Hex(uuid.value[4])
            << Hex(uuid.value[5]) << "-" << Hex(uuid.value[6])
            << Hex(uuid.value[7]) << "-" << Hex(uuid.value[8])
            << Hex(uuid.value[9]) << "-" << Hex(uuid.value[10])
            << Hex(uuid.value[11]) << Hex(uuid.value[12]) << Hex(uuid.value[13])
            << Hex(uuid.value[14]) << Hex(uuid.value[15]);
}

}  // namespace util

}  // namespace ndash
