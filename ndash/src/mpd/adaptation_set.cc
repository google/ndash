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

#include "mpd/adaptation_set.h"

#include "base/logging.h"

namespace ndash {

namespace mpd {

AdaptationSet::AdaptationSet(
    int32_t id,
    AdaptationType type,
    std::vector<std::unique_ptr<Representation>>* representations,
    std::vector<std::unique_ptr<ContentProtection>>* content_protections,
    std::unique_ptr<SegmentBase> segment_base,
    std::vector<std::unique_ptr<DescriptorType>>* supplemental_properties,
    std::vector<std::unique_ptr<DescriptorType>>* essential_properties)
    : id_(id), type_(type), segment_base_(std::move(segment_base)) {
  DCHECK(representations != nullptr);
  representations_ = std::move(*representations);
  if (content_protections != nullptr) {
    content_protections_ = std::move(*content_protections);
  }
  if (supplemental_properties != nullptr) {
    supplemental_properties_ = std::move(*supplemental_properties);
  }
  if (essential_properties != nullptr) {
    essential_properties_ = std::move(*essential_properties);
  }
}

AdaptationSet::~AdaptationSet() {}

bool AdaptationSet::HasContentProtection() const {
  return !content_protections_.empty();
}

const std::vector<std::unique_ptr<Representation>>*
AdaptationSet::GetRepresentations() {
  return &representations_;
}

bool AdaptationSet::HasRepresentations() const {
  return !representations_.empty();
}

int32_t AdaptationSet::NumRepresentations() const {
  return representations_.size();
}

const Representation* AdaptationSet::GetRepresentation(int32_t index) const {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, representations_.size());
  return representations_.at(index).get();
}

const std::vector<std::unique_ptr<ContentProtection>>*
AdaptationSet::GetContentProtections() {
  return &content_protections_;
}

bool AdaptationSet::HasContentProtections() const {
  return !content_protections_.empty();
}

int32_t AdaptationSet::NumContentProtections() const {
  return content_protections_.size();
}

const ContentProtection* AdaptationSet::GetContentProtection(
    int32_t index) const {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, content_protections_.size());
  return content_protections_.at(index).get();
}

int32_t AdaptationSet::GetId() const {
  return id_;
}

AdaptationType AdaptationSet::GetType() const {
  return type_;
}

SegmentBase* AdaptationSet::GetSegmentBase() const {
  return segment_base_.get();
}

size_t AdaptationSet::GetSupplementalPropertyCount() const {
  return supplemental_properties_.size();
}

const DescriptorType* AdaptationSet::GetSupplementalProperty(int index) const {
  return supplemental_properties_.at(index).get();
}

size_t AdaptationSet::GetEssentialPropertyCount() const {
  return essential_properties_.size();
}

const DescriptorType* AdaptationSet::GetEssentialProperty(int index) const {
  return essential_properties_.at(index).get();
}

}  // namespace mpd

}  // namespace ndash
