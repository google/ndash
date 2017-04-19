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

#ifndef NDASH_CHUNK_CHUNK_SOURCE_MOCK_H_
#define NDASH_CHUNK_CHUNK_SOURCE_MOCK_H_

#include <cstdlib>

#include "base/time/time.h"
#include "chunk/chunk_source.h"
#include "gmock/gmock.h"
#include "track_criteria.h"

namespace ndash {
namespace chunk {

class MockChunkSource : public ChunkSourceInterface {
 public:
  MockChunkSource();
  ~MockChunkSource() override;

  MOCK_CONST_METHOD0(CanContinueBuffering, bool());
  MOCK_METHOD0(Prepare, bool());
  MOCK_METHOD0(GetDurationUs, int64_t());
  MOCK_METHOD0(GetContentType, const std::string());
  MOCK_METHOD1(Enable, void(const TrackCriteria*));
  MOCK_METHOD1(ContinueBuffering, void(base::TimeDelta));

  MOCK_METHOD1(OnChunkLoadCompleted, void(Chunk*));
  MOCK_METHOD2(OnChunkLoadError, void(const Chunk*, ChunkLoadErrorReason));
  MOCK_METHOD1(Disable, void(std::deque<std::unique_ptr<MediaChunk>>*));

  void GetChunkOperation(std::deque<std::unique_ptr<MediaChunk>>* media_queue,
                         base::TimeDelta time,
                         ChunkOperationHolder* holder) override;

  // Gives the chunk source something to stick in the media queue on the next
  // call to GetChunkOperation.
  void SetMediaChunk(std::unique_ptr<MediaChunk> chunk);

 private:
  std::unique_ptr<MediaChunk> chunk_;
};

}  // namespace chunk
}  // namespace ndash

#endif  // NDASH_CHUNK_CHUNK_SOURCE_MOCK_H_
