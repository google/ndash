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

#ifndef NDASH_MPD_SEGMENT_TIMELINE_ELEMENT_H_
#define NDASH_MPD_SEGMENT_TIMELINE_ELEMENT_H_

#include <cstdint>

namespace ndash {

namespace mpd {

// Represents a timeline segment from the MPD's SegmentTimeline list.
class SegmentTimelineElement {
 public:
  // The start time in seconds is the division of 'start_time' and 'timescale'
  // of the enclosing element.
  // The duration in seconds is the division of 'duration' and 'timescale'
  // of the enclosing element.
  SegmentTimelineElement(int64_t start_time, int64_t duration)
      : start_time_(start_time), duration_(duration) {}

  int64_t GetStartTime() const { return start_time_; }
  int64_t GetDuration() const { return duration_; }

 private:
  int64_t start_time_;
  int64_t duration_;
};

}  // namespace mpd

}  // namespace ndash

#endif  // NDASH_MPD_SEGMENT_TIMELINE_ELEMENT_H_
