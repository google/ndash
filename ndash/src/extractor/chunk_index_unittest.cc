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

#include <limits>
#include <tuple>

#include "extractor/chunk_index.h"
#include "extractor/chunk_index_unittest.h"
#include "extractor/seek_map.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace ndash {
namespace extractor {

using ::testing::Eq;
using ::testing::IsEmpty;

void GenerateChunkIndexVectors(
    const std::vector<ChunkIndexEntry>& entries,
    std::unique_ptr<std::vector<uint32_t>>* sizes,
    std::unique_ptr<std::vector<uint64_t>>* offsets,
    std::unique_ptr<std::vector<uint64_t>>* durations_us,
    std::unique_ptr<std::vector<uint64_t>>* times_us) {
  sizes->reset(new std::vector<uint32_t>());
  offsets->reset(new std::vector<uint64_t>());
  durations_us->reset(new std::vector<uint64_t>());
  times_us->reset(new std::vector<uint64_t>());

  (*sizes)->reserve(entries.size());
  (*offsets)->reserve(entries.size());
  (*durations_us)->reserve(entries.size());
  (*times_us)->reserve(entries.size());

  for (const ChunkIndexEntry& entry : entries) {
    (*sizes)->push_back(entry.size);
    (*offsets)->push_back(entry.offset);
    (*durations_us)->push_back(entry.duration_us);
    (*times_us)->push_back(entry.time_us);
  }
}

std::unique_ptr<ChunkIndex> GenerateChunkIndex(
    const std::vector<ChunkIndexEntry>& entries) {
  std::unique_ptr<std::vector<uint32_t>> sizes;
  std::unique_ptr<std::vector<uint64_t>> offsets;
  std::unique_ptr<std::vector<uint64_t>> durations_us;
  std::unique_ptr<std::vector<uint64_t>> times_us;

  GenerateChunkIndexVectors(entries, &sizes, &offsets, &durations_us,
                            &times_us);

  std::unique_ptr<ChunkIndex> new_index(
      new ChunkIndex(std::move(sizes), std::move(offsets),
                     std::move(durations_us), std::move(times_us)));

  return new_index;
}

TEST(ChunkIndexTest, TestSingleChunk) {
  constexpr int64_t kTestTime = 256;
  constexpr int64_t kTestOffset = 50;

  std::vector<ChunkIndexEntry> chunk_info = {
      {
          10, kTestOffset, 75, kTestTime,
      },
  };

  const int64_t test_times[] = {
      std::numeric_limits<int64_t>::min(), 0, kTestTime, kTestTime + 10,
      std::numeric_limits<int64_t>::max(),
  };

  std::unique_ptr<ChunkIndex> chunk_index = GenerateChunkIndex(chunk_info);

  EXPECT_THAT(chunk_index->IsSeekable(), Eq(true));

  for (int64_t val : test_times) {
    EXPECT_THAT(chunk_index->GetChunkIndex(val), Eq(0));
    EXPECT_THAT(chunk_index->GetPosition(val), Eq(kTestOffset));
  }
}

TEST(ChunkIndexTest, TestManyChunks) {
  constexpr int32_t kTestSize = 678;      // Not looked at
  constexpr int64_t kTestDuration = 200;  // Not looked at either

  constexpr int64_t kTestTime1 = 256;
  constexpr int64_t kTestOffset1 = 50;

  constexpr int64_t kTestTime2 = 456;     // Continuous
  constexpr int64_t kTestOffset2 = 5000;  // Discontinous

  constexpr int64_t kTestTime3 = 80000;   // Discontinuous
  constexpr int64_t kTestOffset3 = 5678;  // Continuous

  constexpr int64_t kTestTime4 = 5000000;   // Discontinuous
  constexpr int64_t kTestOffset4 = 400000;  // Discontinuous

  constexpr int64_t kTestTime5 = 5000200;   // Continuous
  constexpr int64_t kTestOffset5 = 400678;  // Continuous

  std::vector<ChunkIndexEntry> chunk_info = {
      {
          kTestSize, kTestOffset1, kTestDuration, kTestTime1,
      },
      {
          kTestSize, kTestOffset2, kTestDuration, kTestTime2,
      },
      {
          kTestSize, kTestOffset3, kTestDuration, kTestTime3,
      },
      {
          kTestSize, kTestOffset4, kTestDuration, kTestTime4,
      },
      {
          kTestSize, kTestOffset5, kTestDuration, kTestTime5,
      },
  };

  const struct {
    int64_t time_;
    int32_t index_;
    int64_t pos_;
  } test_cases[] = {
      {std::numeric_limits<int64_t>::min(), 0, kTestOffset1},
      {0, 0, kTestOffset1},
      {kTestTime1, 0, kTestOffset1},
      {kTestTime1 + 10, 0, kTestOffset1},
      {kTestTime2, 1, kTestOffset2},
      {kTestTime2 + 10, 1, kTestOffset2},
      {kTestTime3 - 10, 1, kTestOffset2},
      {kTestTime3, 2, kTestOffset3},
      {kTestTime4 + 50, 3, kTestOffset4},
      {kTestTime5 - 1, 3, kTestOffset4},
      {kTestTime5, 4, kTestOffset5},
      {kTestTime5 + 1, 4, kTestOffset5},
      {std::numeric_limits<int64_t>::max(), 4, kTestOffset5},
  };

  std::unique_ptr<ChunkIndex> chunk_index = GenerateChunkIndex(chunk_info);

  EXPECT_THAT(chunk_index->IsSeekable(), Eq(true));

  for (const auto& test_case : test_cases) {
    EXPECT_THAT(chunk_index->GetChunkIndex(test_case.time_),
                Eq(test_case.index_));
    EXPECT_THAT(chunk_index->GetPosition(test_case.time_), Eq(test_case.pos_));
  }
}

}  // namespace chunk
}  // namespace ndash
