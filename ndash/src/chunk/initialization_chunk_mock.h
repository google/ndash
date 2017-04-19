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

#ifndef NDASH_CHUNK_INITIALIZATION_CHUNK_MOCK_H_
#define NDASH_CHUNK_INITIALIZATION_CHUNK_MOCK_H_

#include <cstdint>

#include "chunk/chunk_extractor_wrapper.h"
#include "chunk/initialization_chunk.h"
#include "gmock/gmock.h"
#include "upstream/data_source.h"
#include "upstream/data_spec.h"
#include "util/format.h"

namespace ndash {

class MediaFormat;

namespace drm {
class DrmInitDataInterface;
}  // namespace drm

namespace extractor {
class ExtractorInputInterface;
class SeekMapInterface;
}  // namespace extractor

namespace upstream {
class DataSpec;
class DataSourceInterface;
}  // namespace upstream

namespace util {
class Format;
}  // namespace util

namespace chunk {

class MockInitializationChunk : public InitializationChunk {
 public:
  MockInitializationChunk(upstream::DataSourceInterface* data_source,
                          const upstream::DataSpec* data_spec,
                          TriggerReason trigger,
                          const util::Format* format,
                          ChunkExtractorWrapper* extractor_wrapper,
                          int parent_id = kNoParentId);
  ~MockInitializationChunk() override;

  // SingleTrackOutputInterface
  MOCK_METHOD1(SetSeekMap, void(const extractor::SeekMapInterface*));
  MOCK_METHOD1(SetDrmInitData, void(const drm::DrmInitDataInterface*));
  MOCK_METHOD1(SetFormat, void(const MediaFormat*));

  // Chunk
  MOCK_CONST_METHOD0(GetNumBytesLoaded, int64_t());

  // Loadable
  MOCK_METHOD0(CancelLoad, void());
  MOCK_CONST_METHOD0(IsLoadCanceled, bool());
  MOCK_METHOD0(Load, bool());
};

}  // namespace chunk
}  // namespace ndash

#endif  // NDASH_CHUNK_CHUNK_MOCK_H_
