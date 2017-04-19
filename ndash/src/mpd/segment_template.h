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

#ifndef NDASH_MPD_SEGMENT_TEMPLATE_H_
#define NDASH_MPD_SEGMENT_TEMPLATE_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "mpd/multi_segment_base.h"
#include "mpd/ranged_uri.h"
#include "mpd/representation.h"
#include "mpd/url_template.h"

namespace ndash {

namespace mpd {

// A MultiSegmentBase that uses a SegmentTemplate to define its segments.
class SegmentTemplate : public MultiSegmentBase {
 public:
  // Construct a new SegmentTemplate.
  // 'initialization' is ignored if initialization_template is non-null.
  // The presentation time offset in seconds is the division of
  // 'presentation_time_offset' and 'timescale'.
  // The duration in seconds is the division of 'duration' and 'timescale'.
  // ('timescale' is in units per second)
  // If segment_timeline is null, all segments are assumed to be of fixed
  // duration as specified by 'duration'.
  // If initialization_template is non-null, then 'initialization' is ignored.
  // One of 'initialization' or 'initialization_template' must be provided.
  // 'start_number' specifies the number of the first Media Segment in the
  // the enclosing Representation in the Period.
  SegmentTemplate(
      std::unique_ptr<std::string> base_url,
      std::unique_ptr<RangedUri> initialization,
      int64_t timescale,
      int64_t presentation_time_offset,
      int32_t start_number,
      int64_t duration,
      std::unique_ptr<std::vector<SegmentTimelineElement>> segment_timeline,
      std::unique_ptr<UrlTemplate> initialization_template,
      std::unique_ptr<UrlTemplate> media_template,
      SegmentTemplate* parent = nullptr);
  ~SegmentTemplate() override;

  std::unique_ptr<RangedUri> GetInitialization(
      const Representation& representation) const override;

  std::unique_ptr<RangedUri> GetSegmentUri(
      const Representation& representation,
      int32_t sequence_number) const override;

  std::unique_ptr<UrlTemplate> GetInitializationTemplate() const;
  std::unique_ptr<UrlTemplate> GetMediaTemplate() const;

  int32_t GetLastSegmentNum(int64_t period_duration_us) const override;

 private:
  std::unique_ptr<UrlTemplate> initialization_template_;
  std::unique_ptr<UrlTemplate> media_template_;
};

}  // namespace mpd

}  // namespace ndash

#endif  // NDASH_MPD_SEGMENT_TEMPLATE_H_
