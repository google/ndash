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

#ifndef NDASH_TIME_RANGE_H_
#define NDASH_TIME_RANGE_H_

#include <cstdint>
#include <utility>

#include "base/time/time.h"

namespace base {
class TickClock;
}  // namespace base

namespace ndash {

// A container to store a start and end time.
class TimeRangeInterface {
 public:
  typedef std::pair<base::TimeDelta, base::TimeDelta> TimeDeltaPair;

  TimeRangeInterface() {}
  virtual ~TimeRangeInterface() {}

  // Whether the range is static, meaning repeated calls to GetCurrentBounds()
  // will return identical results.
  virtual bool IsStatic() const = 0;

  // Returns the start and end times of the TimeRange as a pair:
  // first: start time
  // second: end time
  virtual TimeDeltaPair GetCurrentBounds() const = 0;
};

// A static TimeRange
class StaticTimeRange : public TimeRangeInterface {
 public:
  // start: The beginning of the range.
  // end: The end of the range.
  StaticTimeRange();
  StaticTimeRange(base::TimeDelta start, base::TimeDelta end);
  explicit StaticTimeRange(TimeDeltaPair bounds);
  StaticTimeRange(const StaticTimeRange& other);
  ~StaticTimeRange() override;

  bool IsStatic() const override;
  TimeDeltaPair GetCurrentBounds() const override;

  bool operator==(const StaticTimeRange& other) const {
    return bounds_ == other.bounds_;
  }
  bool operator!=(const StaticTimeRange& other) const {
    return !(*this == other);
  }

 private:
  const StaticTimeRange& operator=(const StaticTimeRange& other) = delete;

  const TimeDeltaPair bounds_;
};

// A dynamic TimeRange, adjusting based on the monotonic system clock.
class DynamicTimeRange : public TimeRangeInterface {
 public:
  // min_start_time: A lower bound on the beginning of the range.
  // max_end_time: An upper bound on the end of the range.
  // time_at_start: The value of clock->NowTicks() corresponding to a media
  //                time of zero.
  // buffer_depth: The buffer depth of the media, or zero.
  // clock: A TickClock (use DefaultTickClock except for testing)
  DynamicTimeRange(base::TimeDelta min_start_time,
                   base::TimeDelta max_end_time,
                   base::TimeTicks time_at_start,
                   base::TimeDelta buffer_depth,
                   base::TickClock* clock);
  DynamicTimeRange(const DynamicTimeRange& other);
  ~DynamicTimeRange() override;

  bool IsStatic() const override;
  TimeDeltaPair GetCurrentBounds() const override;

  bool operator==(const DynamicTimeRange& other) const {
    return min_start_time_ == other.min_start_time_ &&
           max_end_time_ == other.max_end_time_ &&
           time_at_start_ == other.time_at_start_ &&
           buffer_depth_ == other.buffer_depth_;
  }
  bool operator!=(const DynamicTimeRange& other) const {
    return !(*this == other);
  }

 private:
  const DynamicTimeRange& operator=(const DynamicTimeRange& other) = delete;

  base::TimeDelta min_start_time_;
  base::TimeDelta max_end_time_;
  base::TimeTicks time_at_start_;
  base::TimeDelta buffer_depth_;
  base::TickClock* clock_;
};

}  // namespace ndash

#endif  // NDASH_TIME_RANGE_H_
