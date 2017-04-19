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

#include "time_range.h"

#include <algorithm>
#include <cstdint>
#include <utility>

#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "util/util.h"

namespace ndash {

StaticTimeRange::StaticTimeRange()
    : StaticTimeRange(std::make_pair(base::TimeDelta(), base::TimeDelta())) {}

StaticTimeRange::StaticTimeRange(base::TimeDelta start, base::TimeDelta end)
    : StaticTimeRange(std::make_pair(start, end)) {}

StaticTimeRange::StaticTimeRange(TimeDeltaPair bounds) : bounds_(bounds) {
  DCHECK_LE(bounds_.first, bounds_.second);
}

StaticTimeRange::StaticTimeRange(const StaticTimeRange& other)
    : bounds_(other.bounds_) {}

StaticTimeRange::~StaticTimeRange() {}

bool StaticTimeRange::IsStatic() const {
  return true;
}

StaticTimeRange::TimeDeltaPair StaticTimeRange::GetCurrentBounds() const {
  return bounds_;
}

DynamicTimeRange::DynamicTimeRange(base::TimeDelta min_start_time,
                                   base::TimeDelta max_end_time,
                                   base::TimeTicks time_at_start,
                                   base::TimeDelta buffer_depth,
                                   base::TickClock* clock)
    : min_start_time_(min_start_time),
      max_end_time_(max_end_time),
      time_at_start_(time_at_start),
      buffer_depth_(buffer_depth),
      clock_(clock) {}

DynamicTimeRange::DynamicTimeRange(const DynamicTimeRange& other)
    : min_start_time_(other.min_start_time_),
      max_end_time_(other.max_end_time_),
      time_at_start_(other.time_at_start_),
      buffer_depth_(other.buffer_depth_),
      clock_(other.clock_) {}

DynamicTimeRange::~DynamicTimeRange() {}

bool DynamicTimeRange::IsStatic() const {
  return false;
}

DynamicTimeRange::TimeDeltaPair DynamicTimeRange::GetCurrentBounds() const {
  TimeDeltaPair out;

  base::TimeTicks now = clock_->NowTicks();
  base::TimeDelta current_elapsed = now - time_at_start_;
  out.second = std::min(max_end_time_, current_elapsed);

  if (!buffer_depth_.is_zero()) {
    out.first = std::max(min_start_time_, out.second - buffer_depth_);
  } else {
    out.first = min_start_time_;
  }

  return out;
}

}  // namespace ndash
