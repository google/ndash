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

#ifndef NDASH_CHUNK_INITIALIZATION_CHUNK_H_
#define NDASH_CHUNK_INITIALIZATION_CHUNK_H_

#include <cstdint>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "chunk/chunk.h"
#include "chunk/single_track_output.h"
#include "drm/drm_init_data.h"
#include "extractor/seek_map.h"
#include "media_format.h"
#include "upstream/loader.h"

namespace ndash {

class MediaFormat;

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

class ChunkExtractorWrapper;

// A Chunk that uses an Extractor to parse initialization data for single track.
class InitializationChunk : public Chunk, public SingleTrackOutputInterface {
 public:
  // Constructor for a chunk of media samples.
  //
  // data_source: A DataSource for loading the initialization data.
  // data_spec: Defines the initialization data to be loaded.
  // trigger: The reason for this chunk being selected.
  // format: The format of the stream to which this chunk belongs.
  // extractor_wrapper: A wrapped extractor to use for parsing the
  //                    initialization data.
  // parent_id: Identifier for a parent from which this chunk originates.
  InitializationChunk(upstream::DataSourceInterface* data_source,
                      const upstream::DataSpec* data_spec,
                      TriggerReason trigger,
                      const util::Format* format,
                      ChunkExtractorWrapper* extractor_wrapper,
                      int parent_id = kNoParentId);
  ~InitializationChunk() override;

  int64_t GetNumBytesLoaded() const override;

  // True if a MediaFormat was parsed from the chunk. False otherwise.
  // Should be called after loading has completed.
  bool HasFormat() const { return media_format_ != nullptr; }

  // Returns a MediaFormat parsed from the chunk, or null.
  // Should be called after loading has completed.
  std::unique_ptr<const MediaFormat> TakeFormat() {
    return std::move(media_format_);
  }

  // True if a DrmInitData was parsed from the chunk. False otherwise.
  // Should be called after loading has completed.
  bool HasDrmInitData() const { return drm_init_data_ != nullptr; }

  // Returns a DrmInitDataInterface parsed from the chunk, or null.
  // Should be called after loading has completed.
  scoped_refptr<const drm::RefCountedDrmInitData> GetDrmInitData() const {
    return drm_init_data_;
  }

  // True if a SeekMapInterface was parsed from the chunk. False otherwise.
  // Should be called after loading has completed.
  bool HasSeekMap() const { return seek_map_ != nullptr; }

  // Returns a SeekMapInterface parsed from the chunk, or null.
  // Should be called after loading has completed.
  std::unique_ptr<const extractor::SeekMapInterface> TakeSeekMap() {
    return std::move(seek_map_);
  }

  // SingleTrackOutputInterface implementation.
  void GiveSeekMap(
      std::unique_ptr<const extractor::SeekMapInterface> seek_map) override;
  void SetDrmInitData(
      scoped_refptr<const drm::RefCountedDrmInitData> drm_init_data) override;
  void GiveFormat(std::unique_ptr<const MediaFormat> format) override;

  // Required for SingleTrackOutputInterface, but don't call any of the Sample*
  // methods
  bool WriteSampleData(extractor::ExtractorInputInterface* input,
                       size_t max_length,
                       bool allow_end_of_input,
                       int64_t* bytes_appended) override;
  bool WriteSampleDataFixThis(const void* src,
                              size_t length,
                              bool allow_end_of_input,
                              int64_t* num_bytes_written) override;
  void WriteSampleData(const void* data, size_t length) override;
  void WriteSampleMetadata(int64_t time_us,
                           int64_t duration_us,
                           int32_t flags,
                           size_t size,
                           size_t offset,
                           const std::string* encryption_key_id,
                           const std::string* ivs,
                           std::vector<int32_t>* num_bytes_clear,
                           std::vector<int32_t>* num_bytes_enc) override;

  // LoadableInterface implementation
  // Note: BaseMediaChunk::Init() must be called before scheduling this on a
  // Loader, and must not be called again while scheduled in the Loader.
  void CancelLoad() override;
  bool IsLoadCanceled() const override;
  // Runs in loader thread
  bool Load() override;

 private:
  // Only accessed by loader thread, so no locking required
  upstream::DataSourceInterface* data_source_;
  ChunkExtractorWrapper* extractor_wrapper_;

  // Initialization results. Set by the loader thread and read by any thread
  // that knows loading has completed. These variables do not need a lock,
  // since a memory barrier must occur for the reading thread to know that
  // loading has completed.
  std::unique_ptr<const MediaFormat> media_format_;
  scoped_refptr<const drm::RefCountedDrmInitData> drm_init_data_;
  std::unique_ptr<const extractor::SeekMapInterface> seek_map_;

  // Communication in/out of loader thread. Requires lock.
  mutable base::Lock lock_;
  int64_t bytes_loaded_ = 0;
  bool load_canceled_ = false;
};

}  // namespace chunk
}  // namespace ndash

#endif  // NDASH_CHUNK_INITIALIZATION_CHUNK_H_
