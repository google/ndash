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

#include "dash/representation_holder.h"

#include <cstdint>
#include <memory>

#include "base/time/time.h"
#include "chunk/chunk_extractor_wrapper.h"
#include "media_format.h"
#include "mpd/dash_segment_index.h"
#include "mpd/representation.h"

namespace ndash {
namespace dash {

RepresentationHolder::RepresentationHolder(
    base::TimeDelta period_start_time,
    base::TimeDelta period_duration,
    const mpd::Representation* representation,
    std::unique_ptr<chunk::ChunkExtractorWrapper> extractor_wrapper)
    : period_start_time_(period_start_time),
      period_duration_(period_duration),
      extractor_wrapper_(std::move(extractor_wrapper)),
      representation_(representation),
      segment_index_(representation->GetIndex()) {}

RepresentationHolder::~RepresentationHolder() {}

void RepresentationHolder::GiveMediaFormat(
    std::unique_ptr<const MediaFormat> media_format) {
  owned_media_format_ = std::move(media_format);
  media_format_ = owned_media_format_.get();
}
void RepresentationHolder::GiveSegmentIndex(
    std::unique_ptr<const mpd::DashSegmentIndexInterface> segment_index) {
  owned_segment_index_ = std::move(segment_index);
  segment_index_ = owned_segment_index_.get();
}

bool RepresentationHolder::UpdateRepresentation(
    base::TimeDelta new_period_duration,
    const mpd::Representation* new_representation) {
  const mpd::DashSegmentIndexInterface* old_index = representation_->GetIndex();
  const mpd::DashSegmentIndexInterface* new_index =
      new_representation->GetIndex();

  period_duration_ = new_period_duration;
  representation_ = new_representation;
  if (!old_index) {
    // Segment numbers cannot shift if the index isn't defined by the
    // manifest.
    return true;
  }

  if (!old_index->IsExplicit()) {
    // Segment numbers cannot shift if the index isn't explicit.
    return true;
  }

  int32_t old_index_last_segment_num =
      old_index->GetLastSegmentNum(period_duration_.InMicroseconds());
  base::TimeDelta old_index_end_time =
      base::TimeDelta::FromMicroseconds(
          old_index->GetTimeUs(old_index_last_segment_num)) +
      base::TimeDelta::FromMicroseconds(old_index->GetDurationUs(
          old_index_last_segment_num, period_duration_.InMicroseconds()));
  int32_t new_index_first_segment_num = new_index->GetFirstSegmentNum();
  base::TimeDelta new_index_start_time = base::TimeDelta::FromMicroseconds(
      new_index->GetTimeUs(new_index_first_segment_num));
  if (old_index_end_time == new_index_start_time) {
    // The new index continues where the old one ended, with no overlap.
    segment_num_shift_ +=
        old_index->GetLastSegmentNum(period_duration_.InMicroseconds()) -
        new_index_first_segment_num + 1;
    return true;
  }

  if (old_index_end_time < new_index_start_time) {
    // There's a gap between the old index and the new one which means
    // we've slipped behind the live window and can't proceed.
    return false;  // Throw BehindLiveWindowException
  }

  // The new index overlaps with the old one.
  segment_num_shift_ +=
      old_index->GetSegmentNum(new_index_start_time.InMicroseconds(),
                               period_duration_.InMicroseconds()) -
      new_index_first_segment_num;

  return true;
}

int32_t RepresentationHolder::GetSegmentNum(base::TimeDelta position) const {
  return segment_index_->GetSegmentNum(
             (position - period_start_time_).InMicroseconds(),
             period_duration_.InMicroseconds()) +
         segment_num_shift_;
}

base::TimeDelta RepresentationHolder::GetSegmentStartTime(
    int32_t segment_num) const {
  return base::TimeDelta::FromMicroseconds(
             segment_index_->GetTimeUs(segment_num - segment_num_shift_)) +
         period_start_time_;
}

base::TimeDelta RepresentationHolder::GetSegmentEndTime(
    int32_t segment_num) const {
  return GetSegmentStartTime(segment_num) +
         base::TimeDelta::FromMicroseconds(
             segment_index_->GetDurationUs(segment_num - segment_num_shift_,
                                           period_duration_.InMicroseconds()));
}

int32_t RepresentationHolder::GetFirstSegmentNum() const {
  return segment_index_->GetFirstSegmentNum();
}

int32_t RepresentationHolder::GetLastSegmentNum() const {
  return segment_index_->GetLastSegmentNum(period_duration_.InMicroseconds());
}

bool RepresentationHolder::IsBeyondLastSegment(int32_t segment_num) const {
  int last_segment_num = GetLastSegmentNum();
  return last_segment_num == mpd::DashSegmentIndexInterface::kIndexUnbounded
             ? false
             : segment_num > (last_segment_num + segment_num_shift_);
}

bool RepresentationHolder::IsBeforeFirstSegment(int32_t segment_num) const {
  int first_segment_num = GetFirstSegmentNum();
  return segment_num < (first_segment_num + segment_num_shift_);
}

int32_t RepresentationHolder::GetFirstAvailableSegmentNum() const {
  return segment_index_->GetFirstSegmentNum() + segment_num_shift_;
}

std::unique_ptr<mpd::RangedUri> RepresentationHolder::GetSegmentUri(
    int segment_num) const {
  return segment_index_->GetSegmentUrl(segment_num - segment_num_shift_);
}

}  // namespace dash
}  // namespace ndash
