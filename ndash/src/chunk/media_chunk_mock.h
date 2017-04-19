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

#ifndef NDASH_CHUNK_MEDIA_CHUNK_MOCK_H_
#define NDASH_CHUNK_MEDIA_CHUNK_MOCK_H_

#include <cstdint>

#include "chunk/media_chunk.h"
#include "gmock/gmock.h"

namespace ndash {

namespace upstream {
class DataSpec;
}  // namespace upstream

namespace util {
class Format;
}  // namespace util

namespace chunk {

class MockMediaChunk : public MediaChunk {
 public:
  MockMediaChunk(const upstream::DataSpec* data_spec,
                 TriggerReason trigger,
                 const util::Format* format,
                 int64_t start_time_us,
                 int64_t end_time_us,
                 int32_t chunk_index,
                 ParentId parent_id);
  ~MockMediaChunk() override;

  // MediaChunk has no virtual methods

  // Chunk
  MOCK_CONST_METHOD0(GetNumBytesLoaded, int64_t());

  // Loadable
  MOCK_METHOD0(CancelLoad, void());
  MOCK_CONST_METHOD0(IsLoadCanceled, bool());
  MOCK_METHOD0(Load, bool());
};

}  // namespace chunk
}  // namespace ndash

#endif  // NDASH_CHUNK_MEDIA_CHUNK_MOCK_H_
