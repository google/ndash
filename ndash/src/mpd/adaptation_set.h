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

#ifndef NDASH_MPD_ADAPTATION_SET_
#define NDASH_MPD_ADAPTATION_SET_

#include <cstdint>
#include <memory>
#include <vector>

#include "mpd/content_protection.h"
#include "mpd/descriptor_type.h"
#include "mpd/representation.h"
#include "mpd/segment_base.h"

namespace ndash {

namespace mpd {

enum class AdaptationType {
  UNKNOWN = -1,
  VIDEO = 0,
  AUDIO = 1,
  TEXT = 2,
};

class AdaptationSet {
 public:
  // Construct an AdaptationSet with no segment base.
  AdaptationSet(
      int32_t id,
      AdaptationType type,
      std::vector<std::unique_ptr<Representation>>* representations,
      std::vector<std::unique_ptr<ContentProtection>>* content_protections =
          nullptr,
      std::unique_ptr<SegmentBase> segment_base =
          std::unique_ptr<SegmentBase>(nullptr),
      std::vector<std::unique_ptr<DescriptorType>>* supplemental_properties =
          nullptr,
      std::vector<std::unique_ptr<DescriptorType>>* essential_properties =
          nullptr);

  ~AdaptationSet();

  bool HasContentProtection() const;

  // Returns the list of representations in this adaptation set. The pointer
  // is valid only for the lifetime of this adaptation set.
  const std::vector<std::unique_ptr<Representation>>* GetRepresentations();

  bool HasRepresentations() const;
  int32_t NumRepresentations() const;
  const Representation* GetRepresentation(int32_t index) const;

  // Returns the list of content protections in this adaptation set. The
  // pointer is valid only for the lifetime of this adaptation set.
  const std::vector<std::unique_ptr<ContentProtection>>*
  GetContentProtections();

  bool HasContentProtections() const;
  int32_t NumContentProtections() const;
  const ContentProtection* GetContentProtection(int32_t index) const;

  int32_t GetId() const;
  AdaptationType GetType() const;

  SegmentBase* GetSegmentBase() const;

  size_t GetSupplementalPropertyCount() const;
  const DescriptorType* GetSupplementalProperty(int32_t index) const;
  size_t GetEssentialPropertyCount() const;
  const DescriptorType* GetEssentialProperty(int32_t index) const;

 private:
  int32_t id_;
  AdaptationType type_;

  std::vector<std::unique_ptr<Representation>> representations_;
  std::vector<std::unique_ptr<ContentProtection>> content_protections_;

  // A segment base which *may* be referenced by child nodes of this
  // AdaptationSet (unless they have been overridden at their level).
  // May be null.
  std::unique_ptr<SegmentBase> segment_base_;

  std::vector<std::unique_ptr<DescriptorType>> supplemental_properties_;
  std::vector<std::unique_ptr<DescriptorType>> essential_properties_;
};

}  // namespace mpd

}  // namespace ndash

#endif  // NDASH_MPD_ADAPTATION_SET_
