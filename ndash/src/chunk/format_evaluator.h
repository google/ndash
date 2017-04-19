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

#ifndef NDASH_CHUNK_FORMAT_EVALUATOR_H_
#define NDASH_CHUNK_FORMAT_EVALUATOR_H_

#include <deque>
#include <memory>
#include <vector>

#include "base/time/time.h"
#include "chunk/chunk.h"
#include "util/format.h"

namespace ndash {

class PlaybackRate;

namespace chunk {

class MediaChunk;

// A format evaluation.
struct FormatEvaluation {
  // The desired size of the queue.
  int32_t queue_size_ = 0;

  // The sticky reason for the format selection.
  Chunk::TriggerReason trigger_ = Chunk::kTriggerInitial;

  // The selected format.
  std::unique_ptr<util::Format> format_ = nullptr;
};

// Selects from a number of available formats during playback.
class FormatEvaluatorInterface {
 public:
  FormatEvaluatorInterface() {}
  virtual ~FormatEvaluatorInterface() {}

  // Enables the evaluator.
  virtual void Enable() = 0;

  // Disables the evaluator.
  virtual void Disable() = 0;

  // Update the supplied evaluation.
  // When the method is invoked, 'evaluation' will contain the currently
  // selected format (null for the first evaluation), the most recent trigger
  // (TRIGGER_INITIAL for the first evaluation) and the current queue size. The
  // implementation should update these fields as necessary.
  //
  // The trigger should be considered "sticky" for as long as a given
  // representation is selected, and so should only be changed if the
  // representation is also changed.
  //
  // queue: A read only representation of the currently buffered MediaChunks.
  // playback_position: The current playback position.
  // formats: The formats from which to select, in any order.
  // evaluation: The evaluation.
  // playback_rate: The current playback rate
  // TODO: Pass more useful information into this method, and finalize the
  //       interface.
  virtual void Evaluate(const std::deque<std::unique_ptr<MediaChunk>>& queue,
                        base::TimeDelta playback_position,
                        const std::vector<util::Format>& formats,
                        FormatEvaluation* evaluation,
                        const PlaybackRate& playback_rate) const = 0;
};

}  // namespace chunk
}  // namespace ndash

#endif  // NDASH_CHUNK_FORMAT_EVALUATOR_H_
