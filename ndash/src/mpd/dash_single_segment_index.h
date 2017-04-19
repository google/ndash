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

#ifndef NDASH_MPD_DASH_SINGLE_SEGMENT_INDEX_H_
#define NDASH_MPD_DASH_SINGLE_SEGMENT_INDEX_H_

#include <cstdint>
#include <memory>

#include "mpd/dash_segment_index.h"
#include "mpd/ranged_uri.h"

namespace ndash {

namespace mpd {

// A DashSegmentIndexInterface implementation for a SegmentBase that specifies
// only one segment url for the entire media stream.
class DashSingleSegmentIndex : public DashSegmentIndexInterface {
 public:
  DashSingleSegmentIndex(std::unique_ptr<RangedUri> uri);
  ~DashSingleSegmentIndex() override;

  int32_t GetSegmentNum(int64_t time_us,
                        int64_t period_duration_us) const override;

  int64_t GetTimeUs(int32_t segment_num) const override;

  int64_t GetDurationUs(int32_t segment_num,
                        int64_t period_duration_us) const override;

  std::unique_ptr<RangedUri> GetSegmentUrl(int32_t segment_num) const override;

  int32_t GetFirstSegmentNum() const override;

  int32_t GetLastSegmentNum(int64_t period_duration_us) const override;

  bool IsExplicit() const override;

 private:
  std::unique_ptr<RangedUri> uri_;
};

}  // namespace mpd

}  // namespace ndash

#endif  // NDASH_MPD_DASH_SINGLE_SEGMENT_INDEX_H_
