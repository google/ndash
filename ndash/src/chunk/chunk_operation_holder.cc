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

#include "chunk_operation_holder.h"

namespace ndash {
namespace chunk {

ChunkOperationHolder::ChunkOperationHolder()
    : queue_size_(0), chunk_(nullptr), end_of_stream_(false) {}

ChunkOperationHolder::~ChunkOperationHolder() {}

void ChunkOperationHolder::SetQueueSize(int32_t queue_size) {
  queue_size_ = queue_size;
}

void ChunkOperationHolder::SetChunk(std::unique_ptr<Chunk> owned_chunk) {
  owned_chunk_ = std::move(owned_chunk);
  chunk_ = owned_chunk_.get();
}

std::unique_ptr<Chunk> ChunkOperationHolder::TakeChunk() {
  return std::move(owned_chunk_);
}

void ChunkOperationHolder::SetEndOfStream(bool end_of_stream) {
  end_of_stream_ = end_of_stream;
}

void ChunkOperationHolder::Clear() {
  queue_size_ = 0;
  chunk_ = nullptr;
  end_of_stream_ = false;
}

}  // namespace chunk
}  // namespace ndash
