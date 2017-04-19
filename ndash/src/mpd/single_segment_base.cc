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

#include "mpd/single_segment_base.h"

#include "base/logging.h"
#include "util/util.h"

namespace ndash {

namespace mpd {

SingleSegmentBase::SingleSegmentBase(std::unique_ptr<RangedUri> initialization,
                                     int64_t timescale,
                                     int64_t presentation_time_offset,
                                     std::unique_ptr<std::string> uri,
                                     int64_t index_start,
                                     int64_t index_length)
    : SegmentBase(std::move(uri),
                  std::move(initialization),
                  timescale,
                  presentation_time_offset),
      index_start_(index_start),
      index_length_(index_length) {
  DCHECK(base_url_.get() != nullptr);
}

SingleSegmentBase::SingleSegmentBase(std::unique_ptr<std::string> uri)
    : SegmentBase(std::move(uri), nullptr, 1, 0),
      index_start_(0),
      index_length_(-1) {
  DCHECK(base_url_.get() != nullptr);
}

SingleSegmentBase::~SingleSegmentBase() {}

std::unique_ptr<RangedUri> SingleSegmentBase::GetIndex() const {
  return index_length_ <= 0
             ? std::unique_ptr<RangedUri>(nullptr)
             : std::unique_ptr<RangedUri>(new RangedUri(
                   base_url_.get(), "", index_start_, index_length_));
}

bool SingleSegmentBase::IsSingleSegment() const {
  return true;
}

}  // namespace mpd

}  // namespace ndash
