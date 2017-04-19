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

#ifndef NDASH_MPD_SEGMENT_BASE_H_
#define NDASH_MPD_SEGMENT_BASE_H_

#include <cstdint>
#include <memory>

#include "mpd/ranged_uri.h"

namespace ndash {

namespace mpd {

class Representation;

// An approximate representation of a SegmentBase manifest element.
class SegmentBase {
 public:
  // Returns a copy of the ranged uri defining the location of initialization
  // data if such data was given. Otherwise, returns null.
  std::unique_ptr<RangedUri> GetInitializationUri() const;

  // Gets the RangedUri defining the location of initialization data for a given
  // representation. The wrapped pointer may be null if no initialization data
  // exists.
  virtual std::unique_ptr<RangedUri> GetInitialization(
      const Representation& representation) const;

  // Gets the presentation time offset without any scaling.
  int64_t GetPresentationTimeOffset() const;

  // Gets the presentation time offset, in microseconds.
  int64_t GetPresentationTimeOffsetUs() const;

  virtual ~SegmentBase();

  virtual bool IsSingleSegment() const = 0;

  int64_t GetTimeScale() const { return timescale_; }

 protected:
  // Construct a SegmentBase.
  // The presentation time offset in seconds is the division of
  // 'presentation_time_offset' and 'timescale'.
  // ('timescale' is in units per second)
  // initialization may be null if no initialization data exists.
  SegmentBase(std::unique_ptr<std::string> base_url,
              std::unique_ptr<RangedUri> initialization,
              int64_t timescale,
              int64_t presentation_time_offset);

  std::unique_ptr<std::string> base_url_;
  std::unique_ptr<RangedUri> initialization_;
  int64_t timescale_;
  int64_t presentation_time_offset_;
};

}  // namespace mpd

}  // namespace ndash

#endif  // NDASH_MPD_SEGMENT_BASE_H_
