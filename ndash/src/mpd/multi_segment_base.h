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

#ifndef NDASH_MPD_MULTI_SEGMENT_BASE_H_
#define NDASH_MPD_MULTI_SEGMENT_BASE_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "mpd/segment_base.h"
#include "mpd/segment_timeline_element.h"

namespace ndash {

namespace mpd {

// A SegmentBase that consists of multiple segments.
class MultiSegmentBase : public SegmentBase {
 public:
  ~MultiSegmentBase() override;

  int32_t GetSegmentNum(int64_t time_us, int64_t period_duration_us) const;

  // Returns the segment duration for the given sequence_number. Returns -1
  // if the sequence_number is out of range.
  int64_t GetSegmentDurationUs(int32_t sequence_number,
                               int64_t period_duration_us) const;

  // Returns the segment time for the given sequence_number. Returns -1
  // if the sequence_number is out of range.
  int64_t GetSegmentTimeUs(int32_t sequence_number) const;

  // Returns a RangedUri defining the location of a segment for the given index
  // in the given representation.
  // NOTE: Not all implementations own RangedUris to return. Instead they
  // create them on the fly. If a subclass has a RangedUri, it should return
  // a copy for the caller to own.
  virtual std::unique_ptr<RangedUri> GetSegmentUri(
      const Representation& representation,
      int32_t index) const = 0;

  int32_t GetFirstSegmentNum() const;

  virtual int32_t GetLastSegmentNum(int64_t period_duration_us) const = 0;

  virtual bool IsExplicit() const;

  bool IsSingleSegment() const override;

  int32_t GetStartNumber() const { return start_number_; }
  int64_t GetDuration() const { return duration_; }

  // Returns a pointer to the timeline this MultiBaseSegment either owns or
  // has inherited from a parent. May be null if no timeline was provided.
  std::vector<SegmentTimelineElement>* GetSegmentTimeLine() const;

 protected:
  // Construct a new MultiSegmentBase.
  // The presentation time offset in seconds is the division of
  // 'presentation_time_offset' and 'timescale'.
  // The duration in seconds is the division of 'duration' and 'timescale'.
  // ('timescale' is in units per second)
  // If segment_timeline is null, all segments are assumed to be of fixed
  // duration as specified by 'duration'.
  // 'start_number' specifies the number of the first Media Segment in the
  // the enclosing Representation in the Period.
  // 'parent' is an optional pointer to a parent MultiSegmentBase whose
  // properties are shared with this MultiSegmentBase (i.e. timeline)
  MultiSegmentBase(
      std::unique_ptr<std::string> base_url,
      std::unique_ptr<RangedUri> initialization,
      int64_t timescale,
      int64_t presentation_time_offset,
      int32_t start_number,
      int64_t duration,
      std::unique_ptr<std::vector<SegmentTimelineElement>> segment_timeline,
      MultiSegmentBase* parent = nullptr);

  int32_t start_number_;
  int64_t duration_;
  std::unique_ptr<std::vector<SegmentTimelineElement>> segment_timeline_;
  MultiSegmentBase* parent_;
};

}  // namespace mpd

}  // namespace ndash

#endif  // NDASH_MPD_MULTI_SEGMENT_BASE_H_
