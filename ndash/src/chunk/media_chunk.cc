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

#include "chunk/media_chunk.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "util/format.h"

#include <cstdint>

namespace ndash {
namespace chunk {

MediaChunk::MediaChunk(const upstream::DataSpec* data_spec,
                       TriggerReason trigger,
                       const util::Format* format,
                       int64_t start_time_us,
                       int64_t end_time_us,
                       int chunk_index,
                       int parent_id)
    : Chunk(data_spec, kTypeMedia, trigger, format, parent_id),
      start_time_us_(start_time_us),
      end_time_us_(end_time_us),
      chunk_index_(chunk_index) {
  DCHECK(format != nullptr);

  VLOG(3) << "+MediaChunk " << format->GetMimeType() << " ["
          << base::TimeDelta::FromMicroseconds(start_time_us_) << "-"
          << base::TimeDelta::FromMicroseconds(end_time_us_) << "]";
}

MediaChunk::~MediaChunk() {
  VLOG(3) << "-MediaChunk " << format()->GetMimeType() << " ["
          << base::TimeDelta::FromMicroseconds(start_time_us_) << "-"
          << base::TimeDelta::FromMicroseconds(end_time_us_) << "]";
}

}  // namespace chunk
}  // namespace ndash
