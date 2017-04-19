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

#include "mpd/period.h"

#include "base/logging.h"
#include "mpd/segment_base.h"

namespace ndash {

namespace mpd {

Period::Period(
    const std::string& id,
    uint64_t start_ms,
    std::vector<std::unique_ptr<AdaptationSet>>* adaptation_sets,
    std::unique_ptr<SegmentBase> segment_base,
    std::vector<std::unique_ptr<DescriptorType>>* supplemental_properties)
    : id_(id), start_ms_(start_ms), segment_base_(std::move(segment_base)) {
  if (adaptation_sets != nullptr) {
    adaptation_sets_ = std::move(*adaptation_sets);
  }
  if (supplemental_properties != nullptr) {
    supplemental_properties_ = std::move(*supplemental_properties);
  }
}

Period::~Period() {}

std::vector<std::unique_ptr<AdaptationSet>>& Period::GetAdaptationSets() {
  return adaptation_sets_;
}

size_t Period::GetAdaptationSetCount() const {
  return adaptation_sets_.size();
}

const AdaptationSet* Period::GetAdaptationSet(int32_t index) const {
  return adaptation_sets_.at(index).get();
}

const std::string& Period::GetId() const {
  return id_;
}

uint64_t Period::GetStartMs() const {
  return start_ms_;
}

int32_t Period::GetAdaptationSetIndex(AdaptationType type) const {
  for (auto it = adaptation_sets_.begin(); it < adaptation_sets_.end(); it++) {
    if (it->get()->GetType() == type) {
      return it - adaptation_sets_.begin();
    }
  }
  return -1;
}

const SegmentBase* Period::GetSegmentBase() const {
  return segment_base_.get();
}

size_t Period::GetSupplementalPropertyCount() const {
  return supplemental_properties_.size();
}

const DescriptorType* Period::GetSupplementalProperty(int index) const {
  return supplemental_properties_.at(index).get();
}

}  // namespace mpd

}  // namespace ndash
