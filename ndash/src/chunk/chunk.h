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

#ifndef NDASH_CHUNK_CHUNK_H_
#define NDASH_CHUNK_CHUNK_H_

#include "media_format.h"
#include "upstream/data_spec.h"
#include "upstream/loader.h"

namespace ndash {

namespace upstream {
class DataSpec;
}  // namespace upstream

namespace util {
class Format;
}  // namespace util

namespace chunk {

// An base class for Loadable implementations that load chunks of data required
// for the playback of streams.
class Chunk : public upstream::LoadableInterface {
 public:
  // Definition for a callback to be notified when the format is determined.
  typedef base::Callback<void(const MediaFormat* format)> FormatGivenCB;

  typedef int32_t ChunkType;
  typedef int32_t TriggerReason;
  typedef int32_t ParentId;

  // Chunk types. We use constants rather than an enum because custom types can
  // be defined by subclasses.
  static constexpr ChunkType kTypeUnspecified = 0;
  static constexpr ChunkType kTypeMedia = 1;
  static constexpr ChunkType kTypeMediaInitialization = 2;
  static constexpr ChunkType kTypeDrm = 3;
  static constexpr ChunkType kTypeManifest = 4;
  // Implementations may define custom 'type' codes greater than or equal to
  // this value.
  static constexpr ChunkType kTypeCustomBase = 10000;

  // Triggered for an unspecified reason.
  static constexpr TriggerReason kTriggerUnspecified = 0;
  // Triggered by an initial format selection.
  static constexpr TriggerReason kTriggerInitial = 1;
  // Triggered by a user initiated format selection.
  static constexpr TriggerReason kTriggerManual = 2;
  // Triggered by an adaptive format selection.
  static constexpr TriggerReason kTriggerAdaptive = 3;
  // Triggered whilst in a trick play mode.
  static constexpr TriggerReason kTriggerTrickPlay = 4;
  // Implementations may define custom 'trigger' codes greater than or equal to
  // this value.
  static constexpr TriggerReason kTriggerCustomBase = 10000;

  // Value of parent_id_ if no parent id need be specified.
  static constexpr ParentId kNoParentId = -1;

  // TODO(adewhurst): data_spec needs to be held by Chunk, so passing in a
  //                  const pointer is probably not appropriate (caller needs
  //                  to have a temporary sitting around). Consider passing a
  //                  unique_ptr in? Or at least a const reference.
  // data_spec: Defines the data to be loaded. data_spec->length must not
  // exceed std::numeric_limits<int>::max(). If data_spec->length ==
  // LENGTH_UNBOUNDED then the length resolved by DataSource::Open(data_spec)
  // must not exceed std::numeric_limits<int>::max(). Must not be null. A local
  // copy of the DataSpec is stored in data_spec_.
  // TODO(adewhurst): Figure out what limits make sense
  // type: See type_.
  // trigger: See trigger_.
  // format: See format_. Does not take ownership; must remain alive until this
  //         Chunk is deleted.
  // parent_id: See parent_id_.
  Chunk(const upstream::DataSpec* data_spec,
        ChunkType type,
        TriggerReason trigger,
        const util::Format* format,
        ParentId parent_id);
  ~Chunk() override;

  // Gets the number of bytes that have been loaded.
  // Returns the number of bytes that have been loaded.
  virtual int64_t GetNumBytesLoaded() const = 0;

  // Accessors
  ChunkType type() const { return type_; }
  TriggerReason trigger() const { return trigger_; }
  const util::Format* format() const { return format_.get(); }
  const upstream::DataSpec* data_spec() const { return &data_spec_; }
  ParentId parent_id() const { return parent_id_; }

  void SetFormatGivenCallback(FormatGivenCB format_given_cb) {
    format_given_cb_ = format_given_cb;
  }

 protected:
  FormatGivenCB format_given_cb_;

 private:
  // The DataSpec that defines the data to be loaded.
  const upstream::DataSpec data_spec_;
  // The type of the chunk. For reporting only.
  ChunkType type_;
  // The reason why the chunk was generated. For reporting only.
  TriggerReason trigger_;
  // The format associated with the data being loaded, or null if the data
  // being loaded is not associated with a specific format.
  std::unique_ptr<util::Format> format_;
  // Optional identifier for a parent from which this chunk originates.
  ParentId parent_id_;
};

}  // namespace chunk
}  // namespace ndash

#endif  // NDASH_CHUNK_CHUNK_H_
