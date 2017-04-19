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

#include "chunk/media_chunk.h"
#include "chunk/media_chunk_mock.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "upstream/data_spec.h"
#include "util/format.h"

using ::testing::Eq;
using ::testing::IsNull;

namespace ndash {
namespace chunk {

TEST(MediaChunkTest, CanInstantiateMock) {
  upstream::Uri dummy_uri("dummy://");
  upstream::DataSpec data_spec(dummy_uri);
  util::Format format("", "", 0, 0, 0.0, 1, 0, 0, 0);
  constexpr int64_t kTestStartTime = 1;
  constexpr int64_t kTestEndTime = 2;
  constexpr int32_t kTestChunkIndex = 3;

  MockMediaChunk mock_media_chunk(&data_spec, Chunk::kTriggerUnspecified,
                                  &format, kTestStartTime, kTestEndTime,
                                  kTestChunkIndex, Chunk::kNoParentId);

  EXPECT_CALL(mock_media_chunk, GetNumBytesLoaded()).Times(0);

  EXPECT_THAT(mock_media_chunk.type(), Eq(Chunk::kTypeMedia));
  EXPECT_THAT(mock_media_chunk.trigger(), Eq(Chunk::kTriggerUnspecified));
  EXPECT_THAT(mock_media_chunk.data_spec()->DebugString(),
              Eq(data_spec.DebugString()));
  EXPECT_THAT(mock_media_chunk.parent_id(), Eq(Chunk::kNoParentId));

  EXPECT_THAT(mock_media_chunk.start_time_us(), Eq(kTestStartTime));
  EXPECT_THAT(mock_media_chunk.end_time_us(), Eq(kTestEndTime));
  EXPECT_THAT(mock_media_chunk.chunk_index(), Eq(kTestChunkIndex));
}

TEST(MediaChunkTest, GetNextChunkIndex) {
  upstream::Uri dummy_uri("dummy://");
  upstream::DataSpec data_spec(dummy_uri);
  util::Format format("", "", 0, 0, 0.0, 1, 0, 0, 0);

  constexpr int32_t kTestChunkIndex = 3;

  MockMediaChunk mock_media_chunk(&data_spec, Chunk::kTriggerUnspecified,
                                  &format, 0, 0, kTestChunkIndex,
                                  Chunk::kNoParentId);

  EXPECT_THAT(mock_media_chunk.GetNextChunkIndex(), Eq(kTestChunkIndex + 1));
}

TEST(MediaChunkTest, GetPrevChunkIndex) {
  upstream::Uri dummy_uri("dummy://");
  upstream::DataSpec data_spec(dummy_uri);
  util::Format format("", "", 0, 0, 0.0, 1, 0, 0, 0);

  constexpr int32_t kTestChunkIndex = 3;

  MockMediaChunk mock_media_chunk(&data_spec, Chunk::kTriggerUnspecified,
                                  &format, 0, 0, kTestChunkIndex,
                                  Chunk::kNoParentId);

  EXPECT_THAT(mock_media_chunk.GetPrevChunkIndex(), Eq(kTestChunkIndex - 1));
}

}  // namespace chunk
}  // namespace ndash
