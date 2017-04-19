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

#ifndef NDASH_MPD_MULTI_SEGMENT_REPRESENTATION_H_
#define NDASH_MPD_MULTI_SEGMENT_REPRESENTATION_H_

#include <memory>

#include "mpd/multi_segment_base.h"
#include "mpd/representation.h"

namespace ndash {

namespace mpd {

// A DASH representation consisting of multiple segments.
class MultiSegmentRepresentation : public Representation,
                                   public DashSegmentIndexInterface {
 public:
  // content_id Identifies the piece of content to which this representation
  // belongs.
  // revision_id Identifies the revision of the content.
  // format The format of the representation.
  // segment_base The segment base underlying the representation (when
  // the segment base is owned by a higher level).
  // custom_cache_key A custom value to be returned from GetCacheKey(), or "".
  MultiSegmentRepresentation(
      const std::string& content_id,
      int64_t revision_id,
      const util::Format& format,
      MultiSegmentBase* multi_segment_base,
      const std::string& custom_cache_key,
      std::vector<std::unique_ptr<DescriptorType>>* supplemental_properties =
          nullptr,
      std::vector<std::unique_ptr<DescriptorType>>* essential_properties =
          nullptr);

  // content_id Identifies the piece of content to which this representation
  // belongs.
  // revision_id Identifies the revision of the content.
  // format The format of the representation.
  // segment_base_override The segment base underlying the representation (when
  // this representation owns the segment base).
  // custom_cache_key A custom value to be returned from GetCacheKey(), or "".
  MultiSegmentRepresentation(
      const std::string& content_id,
      int64_t revision_id,
      const util::Format& format,
      std::unique_ptr<MultiSegmentBase> segment_base_override,
      const std::string& custom_cache_key,
      std::vector<std::unique_ptr<DescriptorType>>* supplemental_properties =
          nullptr,
      std::vector<std::unique_ptr<DescriptorType>>* essential_properties =
          nullptr);

  ~MultiSegmentRepresentation() override;

  const RangedUri* GetIndexUri() const override;

  const DashSegmentIndexInterface* GetIndex() const override;

  std::unique_ptr<RangedUri> GetSegmentUrl(int segment_index) const override;

  int32_t GetSegmentNum(int64_t time_us,
                        int64_t period_duration_us) const override;

  int64_t GetTimeUs(int32_t segmentIndex) const override;

  int64_t GetDurationUs(int32_t segment_index,
                        int64_t period_duration_us) const override;

  int32_t GetFirstSegmentNum() const override;

  int32_t GetLastSegmentNum(int64_t period_duration_us) const override;

  bool IsExplicit() const override;

  SegmentBase* GetSegmentBase() const override;

 private:
  MultiSegmentBase* multi_segment_base_;
  std::unique_ptr<MultiSegmentBase> segment_base_override_;
};

}  // namespace mpd

}  // namespace ndash

#endif  // NDASH_MPD_MULTI_SEGMENT_REPRESENTATION_H_
