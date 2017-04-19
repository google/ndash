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

#ifndef NDASH_CHUNK_MEDIA_CHUNK_H_
#define NDASH_CHUNK_MEDIA_CHUNK_H_

#include <cstdint>

#include "chunk/chunk.h"

namespace ndash {

namespace upstream {
class DataSpec;
}  // namespace upstream

namespace util {
class Format;
}  // namespace util

namespace chunk {

// A base class for Chunks that contain media samples.
class MediaChunk : public Chunk {
 public:
  // data_spec: Defines the data to be loaded.
  // trigger: The reason for this chunk being selected.
  // format: The format of the stream to which this chunk belongs.
  // start_time_us: The start time of the media contained by the chunk, in
  //                microseconds.
  // end_time_us: The end time of the media contained by the chunk, in
  //              microseconds.
  // chunk_index: The index of the chunk.
  // parent_id: Identifier for a parent from which this chunk originates.
  MediaChunk(const upstream::DataSpec* data_spec,
             TriggerReason trigger,
             const util::Format* format,
             int64_t start_time_us,
             int64_t end_time_us,
             int32_t chunk_index,
             ParentId parent_id);
  ~MediaChunk() override;

  int GetNextChunkIndex() const { return chunk_index_ + 1; }
  int GetPrevChunkIndex() const { return chunk_index_ - 1; }

  // Accessors
  int64_t start_time_us() const { return start_time_us_; }
  int64_t end_time_us() const { return end_time_us_; }
  int32_t chunk_index() const { return chunk_index_; }

 private:
  // The start time of the media contained by the chunk.
  int64_t start_time_us_;
  // The end time of the media contained by the chunk.
  int64_t end_time_us_;
  // The chunk index.
  int32_t chunk_index_;
};

}  // namespace chunk
}  // namespace ndash

#endif  // NDASH_CHUNK_MEDIA_CHUNK_H_
