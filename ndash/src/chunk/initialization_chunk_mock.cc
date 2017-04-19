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

#include "chunk/initialization_chunk_mock.h"

namespace ndash {
namespace chunk {

MockInitializationChunk::MockInitializationChunk(
    upstream::DataSourceInterface* data_source,
    const upstream::DataSpec* data_spec,
    TriggerReason trigger,
    const util::Format* format,
    ChunkExtractorWrapper* extractor_wrapper,
    int parent_id)
    : InitializationChunk(data_source,
                          data_spec,
                          trigger,
                          format,
                          extractor_wrapper,
                          parent_id) {}

MockInitializationChunk::~MockInitializationChunk() {}

}  // namespace chunk
}  // namespace ndash
