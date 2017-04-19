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

#include "chunk/base_media_chunk.h"
#include "extractor/indexed_track_output.h"
#include "extractor/track_output.h"

#include <cstdint>

namespace ndash {
namespace chunk {

BaseMediaChunk::BaseMediaChunk(const upstream::DataSpec* data_spec,
                               TriggerReason trigger,
                               const util::Format* format,
                               int64_t start_time_us,
                               int64_t end_time_us,
                               int chunk_index,
                               bool is_media_format_final,
                               int parent_id)
    : MediaChunk(data_spec,
                 trigger,
                 format,
                 start_time_us,
                 end_time_us,
                 chunk_index,
                 parent_id),
      is_media_format_final_(is_media_format_final) {}

BaseMediaChunk::~BaseMediaChunk() {}

void BaseMediaChunk::Init(extractor::IndexedTrackOutputInterface* output) {
  output_ = output;
  first_sample_index_ = output->GetWriteIndex();
}

}  // namespace chunk
}  // namespace ndash
