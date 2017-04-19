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

#include "mpd/multi_segment_base.h"

#include "base/logging.h"
#include "mpd/dash_segment_index.h"
#include "util/util.h"

namespace ndash {

namespace mpd {

MultiSegmentBase::MultiSegmentBase(
    std::unique_ptr<std::string> base_url,
    std::unique_ptr<RangedUri> initialization,
    int64_t timescale,
    int64_t presentation_time_offset,
    int32_t start_number,
    int64_t duration,
    std::unique_ptr<std::vector<SegmentTimelineElement>> segment_timeline,
    MultiSegmentBase* parent)
    : SegmentBase(std::move(base_url),
                  std::move(initialization),
                  timescale,
                  presentation_time_offset),
      start_number_(start_number),
      duration_(duration),
      segment_timeline_(std::move(segment_timeline)),
      parent_(parent) {}

MultiSegmentBase::~MultiSegmentBase() {}

int32_t MultiSegmentBase::GetSegmentNum(int64_t time_us,
                                        int64_t period_duration_us) const {
  int32_t first_segment_num = GetFirstSegmentNum();
  int32_t low_index = first_segment_num;
  int32_t high_index = GetLastSegmentNum(period_duration_us);
  if (GetSegmentTimeLine() == nullptr) {
    // All segments are of equal duration (with the possible exception of the
    // last one).
    int64_t duration_us = (duration_ * util::kMicrosPerSecond) / timescale_;
    int32_t segment_num = start_number_ + (int32_t)(time_us / duration_us);
    // Ensure we stay within bounds.
    if (segment_num < low_index) {
      return low_index;
    } else {
      // If we're not unbounded, don't go higher than high_index
      if (high_index != DashSegmentIndexInterface::kIndexUnbounded &&
          segment_num > high_index) {
        return high_index;
      }
      // Otherwise, we're good.
      return segment_num;
    }
  } else {
    // The high index cannot be unbounded. Identify the segment using binary
    // search.
    while (low_index <= high_index) {
      int32_t mid_index = (low_index + high_index) / 2;
      int64_t mid_time_us = GetSegmentTimeUs(mid_index);
      if (mid_time_us < time_us) {
        low_index = mid_index + 1;
      } else if (mid_time_us > time_us) {
        high_index = mid_index - 1;
      } else {
        return mid_index;
      }
    }
    return low_index == first_segment_num ? low_index : high_index;
  }
}

int64_t MultiSegmentBase::GetSegmentDurationUs(
    int32_t sequence_number,
    int64_t period_duration_us) const {
  if (GetSegmentTimeLine() != nullptr) {
    int32_t index = sequence_number - start_number_;
    if (index < 0 || index >= GetSegmentTimeLine()->size()) {
      return -1;
    }
    int64_t duration = GetSegmentTimeLine()->at(index).GetDuration();
    return (duration * util::kMicrosPerSecond) / timescale_;
  } else {
    return sequence_number == GetLastSegmentNum(period_duration_us)
               ? (period_duration_us - GetSegmentTimeUs(sequence_number))
               : ((duration_ * util::kMicrosPerSecond) / timescale_);
  }
}

int64_t MultiSegmentBase::GetSegmentTimeUs(int32_t sequence_number) const {
  int64_t unscaled_segment_time;
  int32_t index = sequence_number - start_number_;
  if (GetSegmentTimeLine() != nullptr) {
    if (index < 0 || index >= GetSegmentTimeLine()->size()) {
      return -1;
    }
    unscaled_segment_time = GetSegmentTimeLine()->at(index).GetStartTime() -
                            presentation_time_offset_;
  } else {
    unscaled_segment_time = index * duration_;
  }
  return util::Util::ScaleLargeTimestamp(unscaled_segment_time,
                                         util::kMicrosPerSecond, timescale_);
}

int32_t MultiSegmentBase::GetFirstSegmentNum() const {
  return start_number_;
}

bool MultiSegmentBase::IsExplicit() const {
  return GetSegmentTimeLine() != nullptr;
}

bool MultiSegmentBase::IsSingleSegment() const {
  return false;
}

std::vector<SegmentTimelineElement>* MultiSegmentBase::GetSegmentTimeLine()
    const {
  if (segment_timeline_.get() == nullptr && parent_ != nullptr) {
    return parent_->GetSegmentTimeLine();
  }
  return segment_timeline_.get();
}

}  // namespace mpd

}  // namespace ndash
