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

#ifndef NDASH_UTIL_UUID_H_
#define NDASH_UTIL_UUID_H_

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>

namespace ndash {

namespace util {

// Represents a 128bit (16byte) UUID.
// The empty UUID, returned by Uuid(), is treated as null.
struct Uuid {
  union {
    // The bytes of the UUID, ordered from most to least significant.
    uint8_t value[16];

    // NOTE: Included to force alignment of the byte array.  This shouldn't be
    // accessed directly - instead, use (most|least)_significant_bits.
    uint64_t parts_for_alignment[2];
  };

  // Initializes the UUID to empty.
  Uuid() { memset(value, 0, sizeof(value)); }

  // Initializes the UUID with the given bytes.
  explicit Uuid(const uint8_t other_value[16]) {
    memcpy(value, other_value, sizeof(value));
  }

  // Initializes the UUID with the contents of another.
  Uuid(const Uuid& other) { memcpy(value, other.value, sizeof(value)); }

  // Sets the UUID contents to that of another.
  Uuid& operator=(const Uuid& other) {
    memcpy(value, other.value, sizeof(value));
    return *this;
  }

  // Initializes the UUID from the given string.
  // Expects UUIDs in the form of '09514A5C-F8EB-4B5F-B0C3-97F52B47AE8A'.
  // If the parsing fails the UUID will be initialized to empty.
  explicit Uuid(std::string string) { *this = Parse(string); }

  // Parses the given string form UUID.
  // Expects UUIDs in the form of '09514A5C-F8EB-4B5F-B0C3-97F52B47AE8A'.
  // If the parsing fails an empty UUID will be returned.
  static Uuid Parse(std::string string);

  // Returns the string representation of this instance such that Uuid::Parse
  // can be used to re-create the instance.
  void ToString(std::string* output) const;

  // Accessors for longs representing the most and least significant bits of
  // the UUID.
  uint64_t most_significant_bits() const;
  void set_most_significant_bits(uint64_t value);
  uint64_t least_significant_bits() const;
  void set_least_significant_bits(uint64_t value);

  // Returns true if the UUID is the 'empty' UUID of all zeros.
  bool is_empty() const {
    return !(*reinterpret_cast<const uint64_t*>(&value[0]) ||
             *reinterpret_cast<const uint64_t*>(&value[8]));
  }

  bool operator==(const Uuid& other) const {
    return memcmp(value, other.value, sizeof(value)) == 0;
  }
  bool operator!=(const Uuid& other) const {
    return memcmp(value, other.value, sizeof(value)) != 0;
  }
  bool operator<(const Uuid& other) const {
    return memcmp(value, other.value, sizeof(value)) < 0;
  }
};

static_assert(sizeof(Uuid) == 16, "UUIDs are 128bit values");

std::ostream& operator<<(std::ostream& os, const Uuid& uuid);

}  // namespace util

}  // namespace ndash

#endif  // NDASH_UTIL_UUID_H_
