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

#ifndef NDASH_EXTRACTOR_CHUNK_INDEX_UNITTEST_H_
#define NDASH_EXTRACTOR_CHUNK_INDEX_UNITTEST_H_

#include <cstdint>
#include <memory>
#include <vector>

namespace ndash {
namespace extractor {

class ChunkIndex;

struct ChunkIndexEntry {
  uint32_t size;
  uint64_t offset;
  uint64_t duration_us;
  uint64_t time_us;
};

void GenerateChunkIndexVectors(
    const std::vector<ChunkIndexEntry>& entries,
    std::unique_ptr<std::vector<uint32_t>>* sizes,
    std::unique_ptr<std::vector<uint64_t>>* offsets,
    std::unique_ptr<std::vector<uint64_t>>* durations_us,
    std::unique_ptr<std::vector<uint64_t>>* times_us);

std::unique_ptr<ChunkIndex> GenerateChunkIndex(
    const std::vector<ChunkIndexEntry>& entries);

}  // namespace extractor
}  // namespace ndash

#endif  // NDASH_EXTRACTOR_CHUNK_INDEX_UNITTEST_H_
