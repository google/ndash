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

#ifndef NDASH_UTIL_TIME_H_
#define NDASH_UTIL_TIME_H_

#include <cstdint>

#include "util/util.h"

namespace ndash {
namespace util {

// TODO(adewhurst): Make these more fully featured, like base::Time

constexpr int64_t kUsToTsNumerator = 90;
constexpr int64_t kUsToTsDenominator = 1000;

struct PresentationTime {
  int64_t pts;
};

struct MediaTime {
  int64_t mts;
};

struct MediaDuration {
  int64_t md;
};

inline PresentationTime PresentationTimeFromUs(int64_t us) {
  PresentationTime pt = {
      util::Util::ScaleLargeTimestamp(us, kUsToTsNumerator, kUsToTsDenominator),
  };
  return pt;
}

inline MediaTime MediaTimeFromUs(int64_t us) {
  MediaTime mt = {
      util::Util::ScaleLargeTimestamp(us, kUsToTsNumerator, kUsToTsDenominator),
  };
  return mt;
}

inline MediaDuration MediaDurationFromUs(int64_t us) {
  MediaDuration md = {
      util::Util::ScaleLargeTimestamp(us, kUsToTsNumerator, kUsToTsDenominator),
  };
  return md;
}

}  // namespace util
}  // namespace ndash

#endif  // NDASH_UTIL_TIME_H_
