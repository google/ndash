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

#include "chunk/chunk_source_mock.h"

namespace ndash {
namespace chunk {

MockChunkSource::MockChunkSource() {}
MockChunkSource::~MockChunkSource() {}

void MockChunkSource::GetChunkOperation(
    std::deque<std::unique_ptr<MediaChunk>>* media_queue,
    base::TimeDelta time,
    ChunkOperationHolder* holder) {
  if (chunk_.get() != nullptr) {
    holder->SetChunk(std::move(chunk_));
  }
}

void MockChunkSource::SetMediaChunk(std::unique_ptr<MediaChunk> chunk) {
  chunk_ = std::move(chunk);
}

}  // namespace chunk
}  // namespace ndash
