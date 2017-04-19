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

#include "mpd/segment_template.h"

#include "base/logging.h"
#include "util/util.h"

namespace ndash {

namespace mpd {

SegmentTemplate::SegmentTemplate(
    std::unique_ptr<std::string> base_url,
    std::unique_ptr<RangedUri> initialization,
    int64_t timescale,
    int64_t presentation_time_offset,
    int32_t start_number,
    int64_t duration,
    std::unique_ptr<std::vector<SegmentTimelineElement>> segment_timeline,
    std::unique_ptr<UrlTemplate> initialization_template,
    std::unique_ptr<UrlTemplate> media_template,
    SegmentTemplate* parent)
    : MultiSegmentBase(std::move(base_url),
                       std::move(initialization),
                       timescale,
                       presentation_time_offset,
                       start_number,
                       duration,
                       std::move(segment_timeline),
                       parent),
      initialization_template_(std::move(initialization_template)),
      media_template_(std::move(media_template)) {
  // Either initialization or initialization_template must be non-null but
  // not both. Both can be null.
  // TODO(rmrossi): Consider refactoring this into two subclasses instead of
  // having this confusing logic in one class.
  DCHECK(!(initialization_.get() != nullptr &&
           initialization_template_.get() != nullptr));
  DCHECK(media_template_.get() != nullptr);
}

SegmentTemplate::~SegmentTemplate() {}

std::unique_ptr<RangedUri> SegmentTemplate::GetInitialization(
    const Representation& representation) const {
  if (initialization_template_.get() != nullptr) {
    std::string url_string = initialization_template_->BuildUri(
        representation.GetFormat().GetId(), 0,
        representation.GetFormat().GetBitrate(), 0);
    return std::unique_ptr<RangedUri>(
        new RangedUri(base_url_.get(), url_string, 0, -1));
  } else {
    return MultiSegmentBase::GetInitialization(representation);
  }
}

std::unique_ptr<RangedUri> SegmentTemplate::GetSegmentUri(
    const Representation& representation,
    int32_t sequence_number) const {
  uint64_t time = 0;
  int32_t index = sequence_number - start_number_;
  if (GetSegmentTimeLine() != nullptr) {
    if (index < 0 || index >= GetSegmentTimeLine()->size()) {
      return std::unique_ptr<RangedUri>(nullptr);
    }
    time = GetSegmentTimeLine()->at(index).GetStartTime();
  } else {
    time = (index)*duration_;
  }
  std::string uri_string = media_template_->BuildUri(
      representation.GetFormat().GetId(), sequence_number,
      representation.GetFormat().GetBitrate(), time);
  return std::unique_ptr<RangedUri>(
      new RangedUri(base_url_.get(), uri_string, 0, -1));
}

int32_t SegmentTemplate::GetLastSegmentNum(int64_t period_duration_us) const {
  if (GetSegmentTimeLine() != nullptr) {
    return GetSegmentTimeLine()->size() + start_number_ - 1;
  } else if (period_duration_us == 0) {
    return DashSegmentIndexInterface::kIndexUnbounded;
  } else {
    long durationUs = (duration_ * util::kMicrosPerSecond) / timescale_;
    return start_number_ +
           (int)util::Util::CeilDivide(period_duration_us, durationUs) - 1;
  }
}

std::unique_ptr<UrlTemplate> SegmentTemplate::GetInitializationTemplate()
    const {
  if (initialization_template_.get() == nullptr) {
    return std::unique_ptr<UrlTemplate>(nullptr);
  }
  return std::unique_ptr<UrlTemplate>(
      new UrlTemplate(*initialization_template_.get()));
}

std::unique_ptr<UrlTemplate> SegmentTemplate::GetMediaTemplate() const {
  if (media_template_.get() == nullptr) {
    return std::unique_ptr<UrlTemplate>(nullptr);
  }
  return std::unique_ptr<UrlTemplate>(new UrlTemplate(*media_template_.get()));
}

}  // namespace mpd

}  // namespace ndash
