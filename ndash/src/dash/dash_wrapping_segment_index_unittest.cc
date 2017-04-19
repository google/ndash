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
#include <limits>
#include <string>

#include "dash/dash_wrapping_segment_index.h"
#include "extractor/chunk_index.h"
#include "extractor/chunk_index_unittest.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace ndash {
namespace dash {

using ::testing::Eq;

TEST(DashWrappingSegmentIndexTest, SingleEntryIndex) {
  constexpr int32_t kTestSize = 333;
  constexpr int64_t kTestTime = 256;
  constexpr int64_t kTestOffset = 50;
  constexpr int64_t kTestDuration = 75;
  std::vector<extractor::ChunkIndexEntry> chunk_info = {
      {
          kTestSize, kTestOffset, kTestDuration, kTestTime,
      },
  };

  std::unique_ptr<extractor::ChunkIndex> chunk_index =
      extractor::GenerateChunkIndex(chunk_info);

  const std::string kUri("dummy://");

  DashWrappingSegmentIndex index(std::move(chunk_index), kUri);
  EXPECT_THAT(index.GetSegmentNum(std::numeric_limits<int64_t>::min(), 0),
              Eq(0));
  EXPECT_THAT(index.GetSegmentNum(0, 0), Eq(0));
  EXPECT_THAT(index.GetSegmentNum(kTestTime, 0), Eq(0));
  EXPECT_THAT(index.GetSegmentNum(std::numeric_limits<int64_t>::max(), 0),
              Eq(0));
  EXPECT_THAT(index.GetTimeUs(0), Eq(kTestTime));
  EXPECT_THAT(index.GetDurationUs(0, 0), Eq(kTestDuration));
  EXPECT_THAT(index.GetFirstSegmentNum(), Eq(0));
  EXPECT_THAT(index.GetLastSegmentNum(0), Eq(0));
  EXPECT_THAT(index.IsExplicit(), Eq(true));

  mpd::RangedUri expected_ranged_uri(&kUri, "", kTestOffset, kTestSize);
  std::unique_ptr<mpd::RangedUri> actual_ranged_uri(index.GetSegmentUrl(0));
  EXPECT_THAT(*actual_ranged_uri, Eq(expected_ranged_uri));
}

TEST(DashWrappingSegmentIndexTest, MultipleEntryIndex) {
  constexpr int32_t kTestSize1 = 1024;
  constexpr int64_t kTestTime1 = 0;
  constexpr int64_t kTestOffset1 = 0;
  constexpr int64_t kTestDuration1 = 310;

  constexpr int32_t kTestSize2 = 131072;
  constexpr int64_t kTestTime2 = 310;
  constexpr int64_t kTestOffset2 = 1024;
  constexpr int64_t kTestDuration2 = 665;

  constexpr int32_t kTestSize3 = 8765;
  constexpr int64_t kTestTime3 = 99999;
  constexpr int64_t kTestOffset3 = 77777;
  constexpr int64_t kTestDuration3 = 4321;
  std::vector<extractor::ChunkIndexEntry> chunk_info = {
      {
          kTestSize1, kTestOffset1, kTestDuration1, kTestTime1,
      },
      {
          kTestSize2, kTestOffset2, kTestDuration2, kTestTime2,
      },
      {
          kTestSize3, kTestOffset3, kTestDuration3, kTestTime3,
      },
  };

  std::unique_ptr<extractor::ChunkIndex> chunk_index =
      extractor::GenerateChunkIndex(chunk_info);

  const std::string kUri("dummy://");

  DashWrappingSegmentIndex index(std::move(chunk_index), kUri);

  EXPECT_THAT(index.GetFirstSegmentNum(), Eq(0));
  EXPECT_THAT(index.GetLastSegmentNum(0), Eq(2));
  EXPECT_THAT(index.IsExplicit(), Eq(true));

  EXPECT_THAT(index.GetSegmentNum(std::numeric_limits<int64_t>::min(), 0),
              Eq(0));

  EXPECT_THAT(index.GetSegmentNum(kTestTime1, 0), Eq(0));
  EXPECT_THAT(index.GetSegmentNum(kTestTime1 + kTestDuration1 / 2, 0), Eq(0));
  EXPECT_THAT(index.GetTimeUs(0), Eq(kTestTime1));
  EXPECT_THAT(index.GetDurationUs(0, 0), Eq(kTestDuration1));

  EXPECT_THAT(index.GetSegmentNum(kTestTime2, 0), Eq(1));
  EXPECT_THAT(index.GetSegmentNum(kTestTime2 + kTestDuration2 / 2, 0), Eq(1));
  EXPECT_THAT(index.GetTimeUs(1), Eq(kTestTime2));
  EXPECT_THAT(index.GetDurationUs(1, 0), Eq(kTestDuration2));

  EXPECT_THAT(index.GetSegmentNum(kTestTime3, 0), Eq(2));
  EXPECT_THAT(index.GetSegmentNum(kTestTime3 + kTestDuration3 / 2, 0), Eq(2));
  EXPECT_THAT(index.GetTimeUs(2), Eq(kTestTime3));
  EXPECT_THAT(index.GetDurationUs(2, 0), Eq(kTestDuration3));

  EXPECT_THAT(index.GetSegmentNum(std::numeric_limits<int64_t>::max(), 0),
              Eq(2));

  mpd::RangedUri expected_ranged_uri1(&kUri, "", kTestOffset1, kTestSize1);
  mpd::RangedUri expected_ranged_uri2(&kUri, "", kTestOffset2, kTestSize2);
  mpd::RangedUri expected_ranged_uri3(&kUri, "", kTestOffset3, kTestSize3);
  std::unique_ptr<mpd::RangedUri> actual_ranged_uri1(index.GetSegmentUrl(0));
  std::unique_ptr<mpd::RangedUri> actual_ranged_uri2(index.GetSegmentUrl(1));
  std::unique_ptr<mpd::RangedUri> actual_ranged_uri3(index.GetSegmentUrl(2));
  EXPECT_THAT(*actual_ranged_uri1, Eq(expected_ranged_uri1));
  EXPECT_THAT(*actual_ranged_uri2, Eq(expected_ranged_uri2));
  EXPECT_THAT(*actual_ranged_uri3, Eq(expected_ranged_uri3));
}

}  // namespace dash
}  // namespace ndash
