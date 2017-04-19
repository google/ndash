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

#ifndef NDASH_MPD_DASH_SEGMENT_INDEX_H_
#define NDASH_MPD_DASH_SEGMENT_INDEX_H_

#include <cstdint>

#include "mpd/ranged_uri.h"

namespace ndash {

namespace mpd {

// Indexes the segments within a media stream.
class DashSegmentIndexInterface {
 public:
  static constexpr int32_t kIndexUnbounded = -1;

  DashSegmentIndexInterface() {}
  virtual ~DashSegmentIndexInterface() {}

  // Returns the segment number of the segment containing a given media time.
  // If the given media time is outside the range of the index, then the
  // returned segment number is clamped to GetFirstSegmentNum() (if the given
  // media time is earlier the start of the first segment) or
  // getLastSegmentNum(long) (if the given media time is later then the
  // end of the last segment).
  virtual int32_t GetSegmentNum(int64_t time_us,
                                int64_t period_duration_us) const = 0;

  // Returns the start time of a segment.
  virtual int64_t GetTimeUs(int32_t segment_num) const = 0;

  // Returns the duration of the segment.
  // period_duration_us should be the enclosing period in microseconds or
  // kUnknownTimeUs if the period's duration is not yet known.
  virtual int64_t GetDurationUs(int32_t segment_num,
                                int64_t period_duration_us) const = 0;

  // Returns a RangedUri defining the location of a segment.
  virtual std::unique_ptr<RangedUri> GetSegmentUrl(
      int32_t segment_num) const = 0;

  // Returns the segment number of the first segment.
  virtual int32_t GetFirstSegmentNum() const = 0;

  // Returns the segment number of the last segment, or kIndexUnbounded.
  // An unbounded index occurs if a dynamic manifest uses SegmentTemplate
  // elements without a SegmentTimeline element, and if the period duration is
  // not yet known. In this case the caller must manually determine the window
  // of currently available segments.
  // period_duration_us should be the enclosing period in microseconds or
  // kUnknownTimeUs if the period's duration is not yet known.
  virtual int32_t GetLastSegmentNum(int64_t period_duration_us) const = 0;

  // Returns true if segments are defined explicitly by the index.
  // If true is returned, each segment is defined explicitly by the index data,
  // and all of the listed segments are guaranteed to be available at the time
  // when the index was obtained.
  // If false is returned then segment information was derived from properties
  // such as a fixed segment duration. If the presentation is dynamic, it's
  // possible that only a subset of the segments are available.
  virtual bool IsExplicit() const = 0;
};

}  // namespace mpd

}  // namespace ndash

#endif  // NDASH_MPD_DASH_SEGMENT_INDEX_H_
