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

#ifndef NDASH_UTIL_UTIL_H_
#define NDASH_UTIL_UTIL_H_

#include <cstdint>
#include <string>

namespace ndash {

namespace util {

constexpr int64_t kUnknownTimeUs = -1;
constexpr int64_t kMatchLongestUs = -2;
constexpr int64_t kEndOfTrackUs = -3;
constexpr int32_t kMicrosPerSecond = 1000000;
constexpr int64_t kMicrosPerMs = 1000;

constexpr int64_t kSampleFlagSync = 0x0000001;
constexpr int64_t kSampleFlagEncrypted = 0x0000002;
constexpr int64_t kSampleFlagDecodeOnly = 0x8000000;

constexpr int32_t kResultEndOfInput = -1;

class Util {
 public:
  // Scales a large timestamp.
  //
  // Logically, scaling consists of a multiplication followed by a division.
  // The actual operations performed are designed to minimize the probability
  // of overflow.
  static int64_t ScaleLargeTimestamp(int64_t timestamp,
                                     int64_t multiplier,
                                     int64_t divisor);

  // Divides a {@code numerator} by a {@code denominator}, returning the
  // ceiled result.
  static int64_t CeilDivide(int64_t numerator, int64_t denominator);

  // Parses an xs:dateTime attribute value, returning the parsed timestamp in
  // milliseconds since the epoch.
  static int64_t ParseXsDateTime(const std::string& value);

  // Parses an xs:duration attribute value, returning the parsed duration in
  // milliseconds.  Returns -1 on a parsing error.
  // NOTE: This is not a full implementation of the ISO8601 spec. Durations
  // were not meant to be translated to a single unit (like milliseconds) but
  // rather be added/subtracted from a specific time. However, we expect to
  // only parse seconds so this is okay for our needs.
  // Only P[n]Y[n]M[n]DT[n]H[n]M[n]S format is currently supported.
  //
  // TODO(rmrossi): Support P[n]W format
  // TODO(rmrossi): Support P<date>T<time> format
  static int64_t ParseXsDuration(const std::string& value);

 private:
  Util() = delete;
};

}  // namespace util

}  // namespace ndash

#endif  // NDASH_UTIL_UTIL_H_
