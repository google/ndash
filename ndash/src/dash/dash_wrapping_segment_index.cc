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

#include <cstdint>
#include <memory>
#include <string>

#include "base/logging.h"
#include "dash/dash_wrapping_segment_index.h"
#include "extractor/chunk_index.h"
#include "mpd/ranged_uri.h"

namespace ndash {
namespace dash {

DashWrappingSegmentIndex::DashWrappingSegmentIndex(
    std::unique_ptr<const extractor::ChunkIndex> chunk_index,
    const std::string& uri)
    : chunk_index_(std::move(chunk_index)), uri_(uri) {}

DashWrappingSegmentIndex::~DashWrappingSegmentIndex() {}

int32_t DashWrappingSegmentIndex::GetSegmentNum(
    int64_t time_us,
    int64_t period_duration_us) const {
  return chunk_index_->GetChunkIndex(time_us);
}

int64_t DashWrappingSegmentIndex::GetTimeUs(int32_t segment_num) const {
  DCHECK_GE(segment_num, 0);
  DCHECK_LT(segment_num, chunk_index_->GetChunkCount());
  return chunk_index_->times_us().at(segment_num);
}

int64_t DashWrappingSegmentIndex::GetDurationUs(
    int32_t segment_num,
    int64_t period_duration_us) const {
  DCHECK_GE(segment_num, 0);
  DCHECK_LT(segment_num, chunk_index_->GetChunkCount());
  return chunk_index_->durations_us().at(segment_num);
}

std::unique_ptr<mpd::RangedUri> DashWrappingSegmentIndex::GetSegmentUrl(
    int32_t segment_num) const {
  DCHECK_GE(segment_num, 0);
  DCHECK_LT(segment_num, chunk_index_->GetChunkCount());
  int64_t chunk_offset = chunk_index_->offsets().at(segment_num);
  int64_t chunk_size = chunk_index_->sizes().at(segment_num);
  std::unique_ptr<mpd::RangedUri> new_uri(
      new mpd::RangedUri(&uri_, "", chunk_offset, chunk_size));
  return new_uri;
}

int32_t DashWrappingSegmentIndex::GetFirstSegmentNum() const {
  return 0;
}

int32_t DashWrappingSegmentIndex::GetLastSegmentNum(
    int64_t period_duration_us) const {
  return chunk_index_->GetChunkCount() - 1;
}

bool DashWrappingSegmentIndex::IsExplicit() const {
  return true;
}

}  // namespace dash
}  // namespace ndash
