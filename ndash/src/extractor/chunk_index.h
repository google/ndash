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

#ifndef NDASH_EXTRACTOR_CHUNK_INDEX_H_
#define NDASH_EXTRACTOR_CHUNK_INDEX_H_

#include <cstdint>
#include <vector>

#include "extractor/seek_map.h"

namespace ndash {
namespace extractor {

class ChunkIndex : public SeekMapInterface {
 public:
  // The arguments are a series of vectors, each of which must be the same
  // length. The corresponding elements in each array designate an index entry.
  // Separate vectors are used instead of a vector of "ChunkIndexEntry" (or
  // equivalent) structs because that's how the parser generates the data.
  // sizes: Sizes of the chunks, in bytes
  // offsets: Chunk byte offsets
  // durations_us: Duration of each chunk (microseconds)
  // times_us: Start time of each chunk (microseconds)
  ChunkIndex(std::unique_ptr<std::vector<uint32_t>> sizes,
             std::unique_ptr<std::vector<uint64_t>> offsets,
             std::unique_ptr<std::vector<uint64_t>> durations_us,
             std::unique_ptr<std::vector<uint64_t>> times_us);
  ~ChunkIndex() override;

  // Obtains the index of the chunk corresponding to a given time.
  //
  // time_us: The time, in microseconds.
  // Returns the index of the corresponding chunk.
  int32_t GetChunkIndex(int64_t time_us) const;
  int32_t GetChunkCount() const { return times_us_->size(); }

  const std::vector<uint32_t>& sizes() const { return *sizes_; }
  const std::vector<uint64_t>& offsets() const { return *offsets_; }
  const std::vector<uint64_t>& durations_us() const { return *durations_us_; }
  const std::vector<uint64_t>& times_us() const { return *times_us_; }

  // SeekMap implementation
  bool IsSeekable() const override;
  int64_t GetPosition(int64_t time_us) const override;

 private:
  std::unique_ptr<std::vector<uint32_t>> sizes_;
  std::unique_ptr<std::vector<uint64_t>> offsets_;
  std::unique_ptr<std::vector<uint64_t>> durations_us_;
  std::unique_ptr<std::vector<uint64_t>> times_us_;

  ChunkIndex(const ChunkIndex& other) = delete;
  ChunkIndex& operator=(const ChunkIndex& other) = delete;
};

}  // namespace extractor
}  // namespace ndash

#endif  // NDASH_EXTRACTOR_CHUNK_INDEX_H_
