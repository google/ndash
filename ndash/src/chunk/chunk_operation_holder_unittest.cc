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

#include "chunk/chunk_operation_holder.h"
#include "chunk/chunk_mock.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "upstream/data_spec.h"
#include "upstream/uri.h"

namespace ndash {
namespace chunk {

using ::testing::Eq;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::Return;

TEST(ChunkOperationHolder, ChunkOperationHolder) {
  ChunkOperationHolder holder;

  upstream::Uri ds_uri("http://somewhere");
  upstream::DataSpec spec(ds_uri);

  std::unique_ptr<Chunk> chunk(new MockChunk(&spec, Chunk::kTypeMedia,
                                             Chunk::kTriggerAdaptive, nullptr,
                                             Chunk::kNoParentId));

  holder.SetQueueSize(100);
  holder.SetEndOfStream(false);
  holder.SetChunk(std::move(chunk));

  EXPECT_THAT(holder.GetQueueSize(), Eq(100));
  EXPECT_THAT(holder.IsEndOfStream(), Eq(false));
  EXPECT_THAT(holder.GetChunk(), NotNull());

  holder.SetEndOfStream(true);
  EXPECT_THAT(holder.IsEndOfStream(), Eq(true));

  // Take back ownership of the chunk.
  std::unique_ptr<Chunk> taken_chunk = holder.TakeChunk();
  EXPECT_THAT(taken_chunk.get(), NotNull());

  // ChunkOperationHolder doesn't own the chunk anymore.
  std::unique_ptr<Chunk> re_taken_chunk = holder.TakeChunk();
  EXPECT_THAT(re_taken_chunk.get(), IsNull());

  // Can still get the chunk ptr even though ChunkOperation doesn't own it.
  EXPECT_THAT(holder.GetChunk(), NotNull());

  holder.Clear();
  EXPECT_THAT(holder.GetQueueSize(), Eq(0));
  EXPECT_THAT(holder.IsEndOfStream(), Eq(false));
  EXPECT_THAT(holder.GetChunk(), IsNull());
}

}  // namespace chunk
}  // namespace ndash
