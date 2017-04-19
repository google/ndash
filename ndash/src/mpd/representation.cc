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

#include "mpd/representation.h"

#include <memory>
#include <string>

#include "mpd/multi_segment_representation.h"
#include "mpd/segment_base.h"
#include "mpd/single_segment_representation.h"
#include "util/format.h"

namespace ndash {

namespace mpd {

std::unique_ptr<Representation> Representation::NewInstance(
    const std::string& content_id,
    int64_t revision_id,
    const util::Format& format,
    SegmentBase* segment_base,
    const std::string& custom_cache_key,
    std::vector<std::unique_ptr<DescriptorType>>* supplemental_properties,
    std::vector<std::unique_ptr<DescriptorType>>* essential_properties) {
  // No RTTI so we need to consult IsSingleSegment for this factory.
  if (segment_base->IsSingleSegment()) {
    SingleSegmentBase* single_segment_base =
        static_cast<SingleSegmentBase*>(segment_base);
    return std::unique_ptr<Representation>(new SingleSegmentRepresentation(
        content_id, revision_id, format, single_segment_base, custom_cache_key,
        -1, supplemental_properties, essential_properties));
  } else {
    MultiSegmentBase* multi_segment_base =
        static_cast<MultiSegmentBase*>(segment_base);
    return std::unique_ptr<Representation>(new MultiSegmentRepresentation(
        content_id, revision_id, format, multi_segment_base, custom_cache_key,
        supplemental_properties, essential_properties));
  }
}

std::unique_ptr<Representation> Representation::NewInstance(
    const std::string& content_id,
    int64_t revision_id,
    const util::Format& format,
    std::unique_ptr<SegmentBase> segment_base_override,
    const std::string& custom_cache_key,
    std::vector<std::unique_ptr<DescriptorType>>* supplemental_properties,
    std::vector<std::unique_ptr<DescriptorType>>* essential_properties) {
  // No RTTI so we need to consult IsSingleSegment for this factory.
  if (segment_base_override->IsSingleSegment()) {
    std::unique_ptr<SingleSegmentBase> single_segment_base_override =
        std::unique_ptr<SingleSegmentBase>(
            static_cast<SingleSegmentBase*>(segment_base_override.release()));
    return std::unique_ptr<Representation>(new SingleSegmentRepresentation(
        content_id, revision_id, format,
        std::move(single_segment_base_override), custom_cache_key, -1,
        supplemental_properties, essential_properties));
  } else {
    std::unique_ptr<MultiSegmentBase> multi_segment_base_override =
        std::unique_ptr<MultiSegmentBase>(
            static_cast<MultiSegmentBase*>(segment_base_override.release()));
    return std::unique_ptr<Representation>(new MultiSegmentRepresentation(
        content_id, revision_id, format, std::move(multi_segment_base_override),
        custom_cache_key, supplemental_properties, essential_properties));
  }
}

Representation::Representation(
    const std::string& content_id,
    int64_t revision_id,
    const util::Format& format,
    SegmentBase* segment_base,
    const std::string& custom_cache_key,
    std::vector<std::unique_ptr<DescriptorType>>* supplemental_properties,
    std::vector<std::unique_ptr<DescriptorType>>* essential_properties)
    : format_(format),
      initialization_uri_(segment_base->GetInitialization(*this)),
      content_id_(content_id),
      revision_id_(revision_id) {
  presentation_timeoffset_us_ = segment_base->GetPresentationTimeOffsetUs();
  if (custom_cache_key.length() > 0) {
    cache_key_ = custom_cache_key;
  } else {
    cache_key_.append(content_id);
    cache_key_.append(".");
    cache_key_.append(format.GetId());
    cache_key_.append(".");
    cache_key_.append(std::to_string(revision_id_));
  }
  if (supplemental_properties != nullptr) {
    supplemental_properties_ = std::move(*supplemental_properties);
  }
  if (essential_properties != nullptr) {
    essential_properties_ = std::move(*essential_properties);
  }
}

Representation::~Representation() {}

const RangedUri* Representation::GetInitializationUri() const {
  // May be null.
  return initialization_uri_.get();
}

const std::string& Representation::GetCacheKey() const {
  return cache_key_;
}

size_t Representation::GetSupplementalPropertyCount() const {
  return supplemental_properties_.size();
}

const DescriptorType* Representation::GetSupplementalProperty(int index) const {
  return supplemental_properties_.at(index).get();
}

size_t Representation::GetEssentialPropertyCount() const {
  return essential_properties_.size();
}

const DescriptorType* Representation::GetEssentialProperty(int index) const {
  return essential_properties_.at(index).get();
}

}  // namespace mpd

}  // namespace ndash
