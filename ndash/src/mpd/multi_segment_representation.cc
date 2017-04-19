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

#include "mpd/multi_segment_representation.h"

#include "mpd/multi_segment_base.h"
#include "mpd/single_segment_base.h"

namespace ndash {

namespace mpd {

MultiSegmentRepresentation::MultiSegmentRepresentation(
    const std::string& content_id,
    int64_t revision_id,
    const util::Format& format,
    MultiSegmentBase* multi_segment_base,
    const std::string& custom_cache_key,
    std::vector<std::unique_ptr<DescriptorType>>* supplemental_properties,
    std::vector<std::unique_ptr<DescriptorType>>* essential_properties)
    : Representation(content_id,
                     revision_id,
                     format,
                     multi_segment_base,
                     custom_cache_key,
                     supplemental_properties,
                     essential_properties),
      multi_segment_base_(multi_segment_base) {}

MultiSegmentRepresentation::MultiSegmentRepresentation(
    const std::string& content_id,
    int64_t revision_id,
    const util::Format& format,
    std::unique_ptr<MultiSegmentBase> segment_base_override,
    const std::string& custom_cache_key,
    std::vector<std::unique_ptr<DescriptorType>>* supplemental_properties,
    std::vector<std::unique_ptr<DescriptorType>>* essential_properties)
    : Representation(content_id,
                     revision_id,
                     format,
                     segment_base_override.get(),
                     custom_cache_key,
                     supplemental_properties,
                     essential_properties),
      multi_segment_base_(segment_base_override.get()),
      segment_base_override_(std::move(segment_base_override)) {}

MultiSegmentRepresentation::~MultiSegmentRepresentation() {}

const RangedUri* MultiSegmentRepresentation::GetIndexUri() const {
  return nullptr;
}

const DashSegmentIndexInterface* MultiSegmentRepresentation::GetIndex() const {
  return this;
}

std::unique_ptr<RangedUri> MultiSegmentRepresentation::GetSegmentUrl(
    int32_t segment_index) const {
  return multi_segment_base_->GetSegmentUri(*this, segment_index);
}

int32_t MultiSegmentRepresentation::GetSegmentNum(
    int64_t time_us,
    int64_t period_duration_us) const {
  return multi_segment_base_->GetSegmentNum(time_us, period_duration_us);
}

int64_t MultiSegmentRepresentation::GetTimeUs(int32_t segment_index) const {
  return multi_segment_base_->GetSegmentTimeUs(segment_index);
}

int64_t MultiSegmentRepresentation::GetDurationUs(
    int32_t segment_index,
    int64_t period_duration_us) const {
  return multi_segment_base_->GetSegmentDurationUs(segment_index,
                                                   period_duration_us);
}

int32_t MultiSegmentRepresentation::GetFirstSegmentNum() const {
  return multi_segment_base_->GetFirstSegmentNum();
}

int32_t MultiSegmentRepresentation::GetLastSegmentNum(
    int64_t period_duration_us) const {
  return multi_segment_base_->GetLastSegmentNum(period_duration_us);
}

bool MultiSegmentRepresentation::IsExplicit() const {
  return multi_segment_base_->IsExplicit();
}

SegmentBase* MultiSegmentRepresentation::GetSegmentBase() const {
  return multi_segment_base_;
}

}  // namespace mpd

}  // namespace ndash
