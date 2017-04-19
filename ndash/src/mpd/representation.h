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

#ifndef NDASH_MPD_REPRESENTATION_H_
#define NDASH_MPD_REPRESENTATION_H_

#include <vector>

#include "mpd/dash_segment_index.h"
#include "mpd/descriptor_type.h"
#include "mpd/ranged_uri.h"
#include "util/format.h"

namespace ndash {

namespace mpd {

class SegmentBase;

class MultiSegmentBase;

class Representation {
 public:
  virtual ~Representation();

  // Construct a new Representation instance with a segment base owned by
  // a higher level.
  // content_id Identifies the piece of content to which this representation
  // belongs.
  // revision_id Identifies the revision of the content.
  // format The format of the representation.
  // segment_base A segment base element for the representation (when the
  // segment base is owned by a higher level).
  // custom_cache_key A custom value to be returned from getCacheKey(), or "".
  static std::unique_ptr<Representation> NewInstance(
      const std::string& content_id,
      int64_t revision_id,
      const util::Format& format,
      SegmentBase* segment_base,
      const std::string& custom_cache_key = "",
      std::vector<std::unique_ptr<DescriptorType>>* supplemental_properties =
          nullptr,
      std::vector<std::unique_ptr<DescriptorType>>* essential_properties =
          nullptr);

  // Construct a new Representation instance taking ownership of a segment base.
  // content_id Identifies the piece of content to which this representation
  // belongs.
  // revision_id Identifies the revision of the content.
  // format The format of the representation.
  // segment_base_override A segment base element for the representation (when
  // the segment base should be owned by the new representation).
  // custom_cache_key A custom value to be returned from getCacheKey(), or "".
  static std::unique_ptr<Representation> NewInstance(
      const std::string& content_id,
      int64_t revision_id,
      const util::Format& format,
      std::unique_ptr<SegmentBase> segment_base_override,
      const std::string& custom_cache_key = "",
      std::vector<std::unique_ptr<DescriptorType>>* supplemental_properties =
          nullptr,
      std::vector<std::unique_ptr<DescriptorType>>* essential_properties =
          nullptr);

  // Gets a RangedUri defining the location of the representation's
  // initialization data. May be null if no initialization data exists.
  // The returned reference is valid only for the lifetime of this
  // representation.
  const RangedUri* GetInitializationUri() const;

  // Gets a RangedUri defining the location of the representation's segment
  // index. Null if the representation provides an index directly.
  // The return value is valid only for the lifetime of this representation.
  virtual const RangedUri* GetIndexUri() const = 0;

  // Gets a segment index, if the representation is able to provide one
  // directly. Null if the segment index is defined externally.
  // The return value is valid only for the lifetime of this representation.
  virtual const DashSegmentIndexInterface* GetIndex() const = 0;

  // A cache key for the Representation, in the format
  // {contentId + "." + format.id + "." + revisionId}.
  const std::string& GetCacheKey() const;

  const util::Format& GetFormat() const { return format_; }

  int64_t GetPresentationTimeOffsetUs() const {
    return presentation_timeoffset_us_;
  }

  virtual SegmentBase* GetSegmentBase() const = 0;

  size_t GetSupplementalPropertyCount() const;
  const DescriptorType* GetSupplementalProperty(int32_t index) const;
  size_t GetEssentialPropertyCount() const;
  const DescriptorType* GetEssentialProperty(int32_t index) const;

 protected:
  Representation(
      const std::string& content_id,
      int64_t revision_id,
      const util::Format& format,
      SegmentBase* segment_base,
      const std::string& custom_cache_key,
      std::vector<std::unique_ptr<DescriptorType>>* supplemental_properties =
          nullptr,
      std::vector<std::unique_ptr<DescriptorType>>* essential_properties =
          nullptr);

 private:
  // The format of the representation.
  util::Format format_;

  std::unique_ptr<RangedUri> initialization_uri_;

  // Identifies the piece of content to which this Representation belongs.
  // For example, all Representations belonging to a video should have the same
  // content identifier that uniquely identifies that video.
  const std::string content_id_;

  // Identifies the revision of the content.
  // If the media for a given contentId can change over time without a change
  // to the format's id (e.g. as a result of re-encoding the media with an
  // updated encoder), then this identifier must uniquely identify the revision
  // of the media. The timestamp at which the media was encoded is often a
  // suitable value.
  uint64_t revision_id_;

  // The offset of the presentation timestamps in the media stream relative to
  // media time.
  int64_t presentation_timeoffset_us_;

  std::string cache_key_;

  std::vector<std::unique_ptr<DescriptorType>> supplemental_properties_;
  std::vector<std::unique_ptr<DescriptorType>> essential_properties_;
};

}  // namespace mpd

}  // namespace ndash

#endif  // NDASH_MPD_REPRESENTATION_H_
