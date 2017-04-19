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

#include "dash_single_segment_index.h"

namespace ndash {

namespace mpd {

DashSingleSegmentIndex::DashSingleSegmentIndex(std::unique_ptr<RangedUri> uri)
    : uri_(std::move(uri)) {}

DashSingleSegmentIndex::~DashSingleSegmentIndex() {}

int32_t DashSingleSegmentIndex::GetSegmentNum(
    int64_t time_us,
    int64_t period_duration_us) const {
  return 0;
}

int64_t DashSingleSegmentIndex::GetTimeUs(int32_t segment_num) const {
  return 0;
}

int64_t DashSingleSegmentIndex::GetDurationUs(
    int32_t segment_num,
    int64_t period_duration_us) const {
  return period_duration_us;
}

std::unique_ptr<RangedUri> DashSingleSegmentIndex::GetSegmentUrl(
    int32_t segment_num) const {
  // Always return a copy for the caller to own. Other implementations of
  // DashSegmentIndex may not hold a url, and instead create them on the fly.
  return std::unique_ptr<RangedUri>(new RangedUri(*uri_.get()));
}

int32_t DashSingleSegmentIndex::GetFirstSegmentNum() const {
  return 0;
}

int32_t DashSingleSegmentIndex::GetLastSegmentNum(
    int64_t period_duration_us) const {
  return 0;
}

bool DashSingleSegmentIndex::IsExplicit() const {
  return true;
}

}  // namespace mpd

}  // namespace ndash
