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

#ifndef NDASH_CHUNK_CHUNK_SOURCE_H_
#define NDASH_CHUNK_CHUNK_SOURCE_H_

#include <cstdint>
#include <deque>
#include <memory>
#include <string>

#include "chunk/chunk_operation_holder.h"
#include "chunk/media_chunk.h"
#include "media_format.h"

namespace ndash {

struct TrackCriteria;

namespace chunk {

enum ChunkLoadErrorReason {
  NO_ERROR = 0,
  GENERIC_ERROR,
};

class ChunkSourceInterface {
 public:
  ChunkSourceInterface(){};
  virtual ~ChunkSourceInterface(){};

  // If the source is currently having difficulty providing chunks, then this
  // method returns false, otherwise returns true and does nothing.
  virtual bool CanContinueBuffering() const = 0;

  // Prepares the source.
  // The method can be called repeatedly until the return value indicates
  // success.
  //
  // Returns true if the source was prepared, false otherwise.
  virtual bool Prepare() = 0;

  // Returns the duration of the source, or kUnknownTimeUs if unknown.
  //
  // This method should only be called after the source has been prepared.
  //
  // @return The duration of the source
  virtual int64_t GetDurationUs() = 0;

  // Returns the content type of the source, e.g. "video", "audio", etc.
  //
  // @return The content type of the source
  virtual const std::string GetContentType() = 0;

  // Enable the source with the specified track criteria.
  //
  // This method should only be called after the source has been prepared, and
  // when the source is disabled.
  //
  //  track_criteria The criteria used to select which track chunks should
  //  come from.
  virtual void Enable(const TrackCriteria* track_criteria) = 0;

  // Indicates to the source that it should still be checking for updates to the
  // stream.
  //
  // This method should only be called when the source is enabled.
  //
  //  playbackPositionUs The current playback position.
  virtual void ContinueBuffering(base::TimeDelta playback_position) = 0;

  // Updates the provided ChunkOperationHolder to contain the next
  // operation that should
  // be performed by the calling ChunkSampleSource.
  //
  // This method should only be called when the source is enabled.
  //
  //  queue A representation of the currently buffered MediaChunks.
  //  playbackPositionUs The current playback position. If the queue is empty
  //  then this
  //     parameter is the position from which playback is expected to start (or
  //     restart) and hence
  //     should be interpreted as a seek position.
  //  out A holder for the next operation, whose
  //  ChunkOperationHolder::EndOfStream is
  //     initially set to false, whose ChunkOperationHolder::queue_size_ is
  //     initially equal to
  //     the length of the queue, and whose ChunkOperationHolder::chunk_
  //     is initially equal to
  //     null or a Chunk previously supplied by the ChunkSource
  //     that the caller has
  //     not yet finished loading. In the latter case the chunk can either be
  //     replaced or left
  //     unchanged. Note that leaving the chunk unchanged is both preferred and
  //     more efficient than
  //     replacing it with a new but identical chunk.
  virtual void GetChunkOperation(std::deque<std::unique_ptr<MediaChunk>>* queue,
                                 base::TimeDelta playback_position,
                                 ChunkOperationHolder* out) = 0;

  // Invoked when the ChunkSampleSource has finished loading a chunk
  // obtained from this
  // source.
  //
  // This method should only be called when the source is enabled.
  //
  //  chunk The chunk whose load has been completed.
  virtual void OnChunkLoadCompleted(Chunk* chunk) = 0;

  // Invoked when the ChunkSampleSource encounters an error loading a
  // chunk obtained from
  // this source.
  //
  // This method should only be called when the source is enabled.
  //
  //  chunk The chunk whose load encountered the error.
  //  e The reason for the error.
  virtual void OnChunkLoadError(const Chunk* chunk, ChunkLoadErrorReason e) = 0;

  // Disables the source.
  //
  // This method should only be called when the source is enabled.
  //
  //  queue A representation of the currently buffered MediaChunks.
  virtual void Disable(std::deque<std::unique_ptr<MediaChunk>>* queue) = 0;
};

}  // namespace chunk
}  // namespace ndash

#endif  // NDASH_CHUNK_CHUNK_SOURCE_H_
