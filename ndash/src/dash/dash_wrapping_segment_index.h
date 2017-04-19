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

#ifndef NDASH_DASH_DASH_WRAPPING_SEGMENT_INDEX_H_
#define NDASH_DASH_DASH_WRAPPING_SEGMENT_INDEX_H_

#include <cstdint>
#include <memory>
#include <string>

#include "mpd/dash_segment_index.h"

namespace ndash {

namespace extractor {
class ChunkIndex;
}  // namespace extractor

namespace mpd {
class RangedUri;
}  // namespace mpd

namespace dash {

// An implementation of DashSegmentIndex that wraps a ChunkIndex parsed from a
// media stream.
class DashWrappingSegmentIndex : public mpd::DashSegmentIndexInterface {
 public:
  DashWrappingSegmentIndex(
      std::unique_ptr<const extractor::ChunkIndex> chunk_index,
      const std::string& uri);
  ~DashWrappingSegmentIndex() override;

  int32_t GetSegmentNum(int64_t time_us,
                        int64_t period_duration_us) const override;
  int64_t GetTimeUs(int32_t segment_num) const override;
  int64_t GetDurationUs(int32_t segment_num,
                        int64_t period_duration_us) const override;
  std::unique_ptr<mpd::RangedUri> GetSegmentUrl(
      int32_t segment_num) const override;
  int32_t GetFirstSegmentNum() const override;
  int32_t GetLastSegmentNum(int64_t period_duration_us) const override;
  bool IsExplicit() const override;

 private:
  std::unique_ptr<const extractor::ChunkIndex> chunk_index_;
  std::string uri_;
};

}  // namespace dash
}  // namespace ndash

#endif  // NDASH_DASH_DASH_WRAPPING_SEGMENT_INDEX_H_
