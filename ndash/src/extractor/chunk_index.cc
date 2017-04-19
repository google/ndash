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

#include "extractor/chunk_index.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

#include "base/logging.h"

namespace ndash {
namespace extractor {

ChunkIndex::ChunkIndex(std::unique_ptr<std::vector<uint32_t>> sizes,
                       std::unique_ptr<std::vector<uint64_t>> offsets,
                       std::unique_ptr<std::vector<uint64_t>> durations_us,
                       std::unique_ptr<std::vector<uint64_t>> times_us)
    : sizes_(std::move(sizes)),
      offsets_(std::move(offsets)),
      durations_us_(std::move(durations_us)),
      times_us_(std::move(times_us)) {
  CHECK(sizes_.get());
  CHECK(offsets_.get());
  CHECK(durations_us_.get());
  CHECK(times_us_.get());
  CHECK(!times_us_->empty());
  DCHECK_EQ(sizes_->size(), offsets_->size());
  DCHECK_EQ(sizes_->size(), durations_us_->size());
  DCHECK_EQ(sizes_->size(), times_us_->size());
  DCHECK(std::is_sorted(times_us_->begin(), times_us_->end()));
  // TODO(adewhurst): Check that the chunks describe a contiguous time interval
  //                  without gaps or overlap? It's unclear if ExoPlayer
  //                  expects that to be the case.
}

ChunkIndex::~ChunkIndex() {}

int32_t ChunkIndex::GetChunkIndex(int64_t time_us) const {
  if (time_us < 0) {
    // The times_us_ vector is unsigned, but we accept signed numbers for
    // time_us here. Always return the first chunk in case of negative numbers.
    return 0;
  }

  auto ii = std::lower_bound(times_us_->begin(), times_us_->end(), time_us);

  int32_t index = ii - times_us_->begin();

  if (ii == times_us_->end()) {
    // Can't dereference ii, but because we were given the end, that means the
    // last element must have a start_time_us_ < time_us
    return index - 1;
  }

  // The iterator returned from lower_bound must have *ii >= time_us. We need
  // to return the last chunk with *ii <= time_us.
  //
  // So, for the == case, we just return index. Otherwise return index - 1,
  // except when time_us is before the first chunk's time_us (probably
  // shouldn't happen), in which case we just return the first chunk.

  return *ii == time_us ? index : std::max(0, index - 1);
}

bool ChunkIndex::IsSeekable() const {
  return true;
}

int64_t ChunkIndex::GetPosition(int64_t time_us) const {
  return offsets_->at(GetChunkIndex(time_us));
}

}  // namespace extractor
}  // namespace ndash
