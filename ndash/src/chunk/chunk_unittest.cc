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

#include "chunk/chunk.h"
#include "chunk/chunk_mock.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "upstream/data_spec.h"

using ::testing::Eq;
using ::testing::IsNull;

namespace ndash {
namespace chunk {

TEST(ChunkTest, CanInstantiateMock) {
  upstream::Uri dummy_uri("dummy://");
  upstream::DataSpec data_spec(dummy_uri);

  MockChunk mock_chunk(&data_spec, Chunk::kTypeUnspecified,
                       Chunk::kTriggerUnspecified, nullptr, Chunk::kNoParentId);

  EXPECT_CALL(mock_chunk, GetNumBytesLoaded()).Times(0);

  EXPECT_THAT(mock_chunk.type(), Eq(Chunk::kTypeUnspecified));
  EXPECT_THAT(mock_chunk.trigger(), Eq(Chunk::kTriggerUnspecified));
  EXPECT_THAT(mock_chunk.format(), IsNull());
  EXPECT_THAT(mock_chunk.data_spec()->DebugString(),
              Eq(data_spec.DebugString()));
  EXPECT_THAT(mock_chunk.parent_id(), Eq(Chunk::kNoParentId));
}

}  // namespace chunk
}  // namespace ndash
