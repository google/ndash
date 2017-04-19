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

#include "mpd/segment_base.h"

#include "util/util.h"

namespace ndash {

namespace mpd {

SegmentBase::SegmentBase(std::unique_ptr<std::string> base_url,
                         std::unique_ptr<RangedUri> initialization,
                         int64_t timescale,
                         int64_t presentation_time_offset)
    : base_url_(std::move(base_url)),
      initialization_(std::move(initialization)),
      timescale_(timescale),
      presentation_time_offset_(presentation_time_offset) {}

SegmentBase::~SegmentBase() {}

std::unique_ptr<RangedUri> SegmentBase::GetInitializationUri() const {
  if (initialization_.get() == nullptr) {
    return std::unique_ptr<RangedUri>(nullptr);
  }
  // Note: Subclasses implement this as a factory method.  Even though we are
  // holding onto initialization data, make a copy for caller to own.
  return std::unique_ptr<RangedUri>(new RangedUri(*initialization_.get()));
}

std::unique_ptr<RangedUri> SegmentBase::GetInitialization(
    const Representation& representation) const {
  return GetInitializationUri();
}

int64_t SegmentBase::GetPresentationTimeOffset() const {
  return presentation_time_offset_;
}

int64_t SegmentBase::GetPresentationTimeOffsetUs() const {
  return util::Util::ScaleLargeTimestamp(presentation_time_offset_,
                                         util::kMicrosPerSecond, timescale_);
}

}  // namespace mpd

}  // namespace ndash
