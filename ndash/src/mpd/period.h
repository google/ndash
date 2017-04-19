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

#ifndef NDASH_MPD_PERIOD_
#define NDASH_MPD_PERIOD_

#include <cstdint>
#include <memory>
#include <vector>

#include "mpd/adaptation_set.h"
#include "mpd/descriptor_type.h"

namespace ndash {

namespace mpd {

class SegmentBase;

class Period {
 public:
  Period(const std::string& id,
         uint64_t start,
         std::vector<std::unique_ptr<AdaptationSet>>* adaptation_sets = nullptr,
         std::unique_ptr<SegmentBase> segment_base =
             std::unique_ptr<SegmentBase>(nullptr),
         std::vector<std::unique_ptr<DescriptorType>>* supplemental_properties =
             nullptr);
  ~Period();

  // Returns a reference to the list of adaptation sets. The reference is
  // valid only for the lifetime of this Period.
  std::vector<std::unique_ptr<AdaptationSet>>& GetAdaptationSets();

  size_t GetAdaptationSetCount() const;

  const AdaptationSet* GetAdaptationSet(int32_t index) const;

  // The returned reference is valid only for the lifetime of this Period.
  const std::string& GetId() const;

  uint64_t GetStartMs() const;

  // Returns the index of the first adaptation set of a given type, or -1 if no
  // adaptation set of the specified type exists.
  int32_t GetAdaptationSetIndex(AdaptationType type) const;

  const SegmentBase* GetSegmentBase() const;

  size_t GetSupplementalPropertyCount() const;
  const DescriptorType* GetSupplementalProperty(int32_t index) const;

 private:
  // The period identifier, if one exists.
  std::string id_;

  // The start time of the period in milliseconds.
  uint64_t start_ms_;

  // The adaptation sets belonging to the period.
  std::vector<std::unique_ptr<AdaptationSet>> adaptation_sets_;

  // A segment base which *may* be referenced by child nodes of this
  // Period (unless they have been overridden at their level).  May be null.
  std::unique_ptr<SegmentBase> segment_base_;

  std::vector<std::unique_ptr<DescriptorType>> supplemental_properties_;
};

}  // namespace mpd

}  // namespace ndash

#endif  // NDASH_MPD_PERIOD_
