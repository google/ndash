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

#include "chunk/chunk.h"
#include "util/format.h"

#include "base/logging.h"

namespace ndash {
namespace chunk {

constexpr Chunk::ChunkType Chunk::kTypeUnspecified;
constexpr Chunk::ChunkType Chunk::kTypeMedia;
constexpr Chunk::ChunkType Chunk::kTypeMediaInitialization;
constexpr Chunk::ChunkType Chunk::kTypeDrm;
constexpr Chunk::ChunkType Chunk::kTypeManifest;
constexpr Chunk::ChunkType Chunk::kTypeCustomBase;
constexpr Chunk::TriggerReason Chunk::kTriggerUnspecified;
constexpr Chunk::TriggerReason Chunk::kTriggerInitial;
constexpr Chunk::TriggerReason Chunk::kTriggerManual;
constexpr Chunk::TriggerReason Chunk::kTriggerAdaptive;
constexpr Chunk::TriggerReason Chunk::kTriggerTrickPlay;
constexpr Chunk::TriggerReason Chunk::kTriggerCustomBase;
constexpr Chunk::ParentId Chunk::kNoParentId;

Chunk::Chunk(const upstream::DataSpec* data_spec,
             ChunkType type,
             TriggerReason trigger,
             const util::Format* format,
             ParentId parent_id)
    : data_spec_(*data_spec),
      type_(type),
      trigger_(trigger),
      format_(format ? new util::Format(*format) : nullptr),
      parent_id_(parent_id) {
  DCHECK(data_spec != nullptr);
}

Chunk::~Chunk() {}

}  // namespace chunk
}  // namespace ndash
