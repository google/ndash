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

#ifndef NDASH_MPD_SINGLE_SEGMENT_REPRESENTATION_H_
#define NDASH_MPD_SINGLE_SEGMENT_REPRESENTATION_H_

#include <cstdint>
#include <memory>
#include <string>

#include "mpd/dash_single_segment_index.h"
#include "mpd/representation.h"
#include "mpd/single_segment_base.h"
#include "util/format.h"

namespace ndash {

namespace mpd {

// A DASH representation consisting of a single segment.
class SingleSegmentRepresentation : public Representation {
 public:
  // content_id Identifies the piece of content to which this representation
  // belongs.
  // revision_id Identifies the revision of the content.
  // format The format of the representation.
  // uri The uri of the media.
  // initialization_start The offset of the first byte of initialization data.
  // initialization_end The offset of the last byte of initialization data.
  // index_start The offset of the first byte of index data.
  // index_end The offset of the last byte of index data.
  // custom_cache_key A custom value to be returned from getCacheKey(), or null.
  // content_length The content length, or -1 if unknown.
  static SingleSegmentRepresentation* NewInstance(
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
      std::vector<std::unique_ptr<DescriptorType>>* supplemental_properties =
          nullptr,
      std::vector<std::unique_ptr<DescriptorType>>* essential_properties =
          nullptr);

  const RangedUri* GetIndexUri() const override;

  const DashSegmentIndexInterface* GetIndex() const override;

  // content_id Identifies the piece of content to which this representation
  // belongs.
  // revision_id Identifies the revision of the content.
  // format The format of the representation.
  // segment_base The segment base underlying the representation (when the
  // segment base is owned by a higher level).
  // custom_cache_key A custom value to be returned from {@link #getCacheKey()},
  // or null.
  // content_length The content length, or -1 if unknown.
  SingleSegmentRepresentation(
      const std::string& content_id,
      int64_t revision_id,
      const util::Format& format,
      SingleSegmentBase* single_segment_base,
      const std::string& custom_cache_key,
      int64_t content_length,
      std::vector<std::unique_ptr<DescriptorType>>* supplemental_properties =
          nullptr,
      std::vector<std::unique_ptr<DescriptorType>>* essential_properties =
          nullptr);

  // content_id Identifies the piece of content to which this representation
  // belongs.
  // revision_id Identifies the revision of the content.
  // format The format of the representation.
  // segment_base The segment base underlying the representation (when this
  // representation should own the segment base).
  // custom_cache_key A custom value to be returned from {@link #getCacheKey()},
  // or null.
  // content_length The content length, or -1 if unknown.
  SingleSegmentRepresentation(
      const std::string& content_id,
      int64_t revision_id,
      const util::Format& format,
      std::unique_ptr<SingleSegmentBase> segment_base_override,
      const std::string& custom_cache_key,
      int64_t content_length,
      std::vector<std::unique_ptr<DescriptorType>>* supplemental_properties =
          nullptr,
      std::vector<std::unique_ptr<DescriptorType>>* essential_properties =
          nullptr);

  ~SingleSegmentRepresentation() override;

  SegmentBase* GetSegmentBase() const override;

 private:
  SingleSegmentBase* single_segment_base_;
  std::unique_ptr<SingleSegmentBase> segment_base_override_;
  std::unique_ptr<RangedUri> index_uri_;
  std::unique_ptr<DashSingleSegmentIndex> segment_index_;

  // The uri of the single segment. Storage is owned by SingleSegmentBase.
  // TODO(rmrossi): This appears to be unused so far. Remove this since it can
  // be retrieved from the segment base anyway.
  const std::string* uri_;

  // The content length, or -1 if unknown.
  int64_t content_length_;
};

}  // namespace mpd

}  // namespace ndash

#endif  // NDASH_MPD_SINGLE_SEGMENT_REPRESENTATION_H_
