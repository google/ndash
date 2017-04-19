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

#include "mpd/segment_list.h"

#include "base/logging.h"

namespace ndash {

namespace mpd {

SegmentList::SegmentList(
    std::unique_ptr<std::string> base_url,
    std::unique_ptr<RangedUri> initialization,
    int64_t timescale,
    int64_t presentation_time_offset,
    int32_t start_number,
    int64_t duration,
    std::unique_ptr<std::vector<SegmentTimelineElement>> segment_timeline,
    std::unique_ptr<std::vector<RangedUri>> media_segments,
    SegmentList* parent)
    : MultiSegmentBase(std::move(base_url),
                       std::move(initialization),
                       timescale,
                       presentation_time_offset,
                       start_number,
                       duration,
                       std::move(segment_timeline),
                       parent),
      media_segments_(std::move(media_segments)),
      parent_(parent) {
  if (parent_ == nullptr && media_segments_.get() == nullptr) {
    // If we were given neither parent nor media list, make an empty list.
    media_segments_ =
        std::unique_ptr<std::vector<RangedUri>>(new std::vector<RangedUri>());
  }
  DCHECK(GetMediaSegments() != nullptr);
}

SegmentList::~SegmentList() {}

std::unique_ptr<RangedUri> SegmentList::GetSegmentUri(
    const Representation& representation,
    int32_t sequence_number) const {
  // NOTE: Even though we have a ranged uri, we return a copy. See
  // multi_segment_base.h.
  int index = sequence_number - start_number_;
  if (index >= 0 && index < GetMediaSegments()->size()) {
    return std::unique_ptr<RangedUri>(
        new RangedUri(GetMediaSegments()->at(sequence_number - start_number_)));
  } else {
    return std::unique_ptr<RangedUri>(nullptr);
  }
}

int32_t SegmentList::GetLastSegmentNum(int64_t period_duration_us) const {
  return start_number_ + GetMediaSegments()->size() - 1;
}

bool SegmentList::IsExplicit() const {
  return true;
}

std::vector<RangedUri>* SegmentList::GetMediaSegments() const {
  if (media_segments_.get() == nullptr && parent_ != nullptr) {
    return parent_->GetMediaSegments();
  }
  return media_segments_.get();
}

}  // namespace mpd

}  // namespace ndash
