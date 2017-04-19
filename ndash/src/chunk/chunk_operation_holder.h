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

#ifndef NDASH_CHUNK_OPERATION_HOLDER_H_
#define NDASH_CHUNK_OPERATION_HOLDER_H_

#include <cstdint>
#include <memory>

#include "chunk.h"

namespace ndash {
namespace chunk {

// Holds a chunk operation, which consists of a either:
// 1. The number of MediaChunk objects that should be retained on the queue
// together with the next Chunk to load. Chunk may be null if the next chunk
// cannot be provided yet.
// 2. A flag indicating that the end of the stream has been reached.
class ChunkOperationHolder {
 public:
  ChunkOperationHolder();
  virtual ~ChunkOperationHolder();

  // Clears the holder.
  void Clear();

  void SetQueueSize(int32_t queue_size);
  int32_t GetQueueSize() const { return queue_size_; }
  void SetChunk(std::unique_ptr<Chunk> owned_chunk);

  // Return a pointer to the chunk this holder points to. The holder may not
  // own the storage space for the chunk returned.
  Chunk* GetChunk() const { return chunk_; }

  // Take storage ownership of the chunk away from this holder.
  // This holder will still hold a pointer to the chunk.
  // TODO(rmrossi): This method was added because the original code queued
  // media chunks coming out of the holder but left the holder still pointing
  // to the chunk.  It may be the case that we can simply null out the pointer
  // after doing so because the code eventually overwrites the pointer anyway
  // before ever calling GetChunk again. Figure out whether this is the case
  // and alter TakeChunk accordingly.
  std::unique_ptr<Chunk> TakeChunk();

  void SetEndOfStream(bool end_of_stream);
  bool IsEndOfStream() const { return end_of_stream_; }

 private:
  // The number of MediaChunk objects to retain in a queue.
  int32_t queue_size_;

  Chunk* chunk_;

  // The chunk when storage is owned by this holder. Ownership can be taken
  // away (to be placed into a queue, for example) but the holder may still
  // point to the chunk via chunk_ pointer.
  std::unique_ptr<Chunk> owned_chunk_;

  // Indicates that the end of the stream has been reached.
  bool end_of_stream_;
};

}  // namespace chunk
}  // namespace ndash

#endif  // NDASH_CHUNK_OPERATION_HOLDER_H_
