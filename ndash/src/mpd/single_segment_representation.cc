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
#include "mpd/single_segment_representation.h"

#include <cstdint>
#include <memory>
#include <string>

#include "mpd/multi_segment_base.h"
#include "mpd/segment_base.h"
#include "mpd/single_segment_base.h"

namespace ndash {

namespace mpd {

SingleSegmentRepresentation* SingleSegmentRepresentation::NewInstance(
    const std::string& content_id,
    int64_t revision_id,
    const util::Format& format,
    const std::string& uri,
    int64_t initialization_start,
    int64_t initialization_end,
    int64_t index_start,
    int64_t index_end,
    const std::string& custom_cache_key,
    int64_t content_length,
    std::vector<std::unique_ptr<DescriptorType>>* supplemental_properties,
    std::vector<std::unique_ptr<DescriptorType>>* essential_properties) {
  // Make a copy of the provided uri. We need to create the initialization uri
  // before the segment base (which will own the storage space for uri).
  std::unique_ptr<std::string> representation_uri =
      std::unique_ptr<std::string>(new std::string(uri));
  std::unique_ptr<RangedUri> initialization_uri(
      new RangedUri(representation_uri.get(), "", initialization_start,
                    initialization_end - initialization_start + 1));
  std::unique_ptr<SingleSegmentBase> segment_base_override(
      new SingleSegmentBase(std::move(initialization_uri), 1, 0,
                            std::move(representation_uri), index_start,
                            index_end - index_start + 1));

  return new SingleSegmentRepresentation(
      content_id, revision_id, format, std::move(segment_base_override),
      custom_cache_key, content_length, supplemental_properties,
      essential_properties);
}

SingleSegmentRepresentation::SingleSegmentRepresentation(
    const std::string& content_id,
    int64_t revision_id,
    const util::Format& format,
    SingleSegmentBase* single_segment_base,
    const std::string& custom_cache_key,
    int64_t content_length,
    std::vector<std::unique_ptr<DescriptorType>>* supplemental_properties,
    std::vector<std::unique_ptr<DescriptorType>>* essential_properties)
    : Representation(content_id,
                     revision_id,
                     format,
                     single_segment_base,
                     custom_cache_key,
                     supplemental_properties,
                     essential_properties),
      single_segment_base_(single_segment_base),
      uri_(single_segment_base_->GetUri()) {
  index_uri_ = single_segment_base_->GetIndex();
  content_length_ = content_length;
  // If we have an index uri then the index is defined externally, and we
  // shouldn't return one directly. If we don't, then we can't do better than
  // an index defining a single segment.
  if (index_uri_.get() == nullptr) {
    std::unique_ptr<RangedUri> ranged_uri(
        new RangedUri(single_segment_base_->GetUri(), "", 0, content_length));
    segment_index_.reset(new DashSingleSegmentIndex(std::move(ranged_uri)));
  }
}

SingleSegmentRepresentation::SingleSegmentRepresentation(
    const std::string& content_id,
    int64_t revision_id,
    const util::Format& format,
    std::unique_ptr<SingleSegmentBase> segment_base_override,
    const std::string& custom_cache_key,
    int64_t content_length,
    std::vector<std::unique_ptr<DescriptorType>>* supplemental_properties,
    std::vector<std::unique_ptr<DescriptorType>>* essential_properties)
    : Representation(content_id,
                     revision_id,
                     format,
                     segment_base_override.get(),
                     custom_cache_key,
                     supplemental_properties,
                     essential_properties),
      single_segment_base_(segment_base_override.get()),
      segment_base_override_(std::move(segment_base_override)),
      uri_(single_segment_base_->GetUri()) {
  index_uri_ = single_segment_base_->GetIndex();
  content_length_ = content_length;
  // If we have an index uri then the index is defined externally, and we
  // shouldn't return one directly. If we don't, then we can't do better than
  // an index defining a single segment.
  if (index_uri_.get() == nullptr) {
    std::unique_ptr<RangedUri> ranged_uri(
        new RangedUri(single_segment_base_->GetUri(), "", 0, content_length));
    segment_index_.reset(new DashSingleSegmentIndex(std::move(ranged_uri)));
  }
}

SingleSegmentRepresentation::~SingleSegmentRepresentation() {
  index_uri_ = nullptr;
}

const RangedUri* SingleSegmentRepresentation::GetIndexUri() const {
  return index_uri_.get();
}

const DashSegmentIndexInterface* SingleSegmentRepresentation::GetIndex() const {
  return segment_index_.get();
}

SegmentBase* SingleSegmentRepresentation::GetSegmentBase() const {
  return single_segment_base_;
}

}  // namespace mpd

}  // namespace ndash
