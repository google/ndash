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

#ifndef NDASH_CHUNK_CONTAINER_MEDIA_CHUNK_H_
#define NDASH_CHUNK_CONTAINER_MEDIA_CHUNK_H_

#include <cstdint>
#include <memory>

#include "base/memory/ref_counted.h"
#include "base/synchronization/cancellation_flag.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "chunk/base_media_chunk.h"
#include "chunk/single_track_output.h"

namespace ndash {

class MediaFormat;

namespace drm {
class RefCountedDrmInitData;
}  // namespace drm

namespace extractor {
class ExtractorInterface;
class ExtractorInputInterface;
class SeekMapInterface;
}  // namespace extractor

namespace upstream {
class DataSourceInterface;
class DataSpec;
}  // namespace upstream

namespace util {
class Format;
}  // namespace util

namespace chunk {

class ChunkExtractorWrapper;

// A BaseMediaChunk that uses an ExtractorInterface to parse sample data.
class ContainerMediaChunk : public BaseMediaChunk,
                            public SingleTrackOutputInterface {
 public:
  // data_source: A DataSource for loading the data.
  // data_spec: Defines the data to be loaded.
  // trigger: The reason for this chunk being selected.
  // format: The format of the stream to which this chunk belongs.
  // start_time_us: The start time of the media contained by the chunk, in
  //                microseconds.
  // end_time_us: The end time of the media contained by the chunk, in
  //              microseconds.
  // chunk_index: The index of the chunk.
  // sample_offset: An offset to add to the sample timestamps parsed by the
  //                extractor.
  // extractor_wrapper: A wrapped extractor to use for parsing the
  //                    initialization data.
  // media_format: The MediaFormat of the chunk, if known. May be null if the
  //               data is known to define its own format.
  // drm_init_data: The DrmInitDataInterface for the sample. Null if the sample
  //                is not drm protected. May also be null if the data is known
  //                to define its own initialization data.
  // is_media_format_final: True if media_format and drm_init_data are known to
  //                        be correct and final. False if the data may define
  //                        its own format or initialization data.
  // parent_id: Identifier for a parent from which this chunk originates.
  ContainerMediaChunk(
      upstream::DataSourceInterface* data_source,
      const upstream::DataSpec* data_spec,
      TriggerReason trigger,
      const util::Format* format,
      int64_t start_time_us,
      int64_t end_time_us,
      int32_t chunk_index,
      base::TimeDelta sample_offset,
      scoped_refptr<ChunkExtractorWrapper> extractor_wrapper,
      std::unique_ptr<const MediaFormat> media_format,
      scoped_refptr<const drm::RefCountedDrmInitData> drm_init_data,
      bool is_media_format_final,
      int parent_id);
  ~ContainerMediaChunk() override;

  // BaseMediaChunk implementation
  const MediaFormat* GetMediaFormat() const override;
  scoped_refptr<const drm::RefCountedDrmInitData> GetDrmInitData()
      const override;

  // Chunk implementation
  int64_t GetNumBytesLoaded() const override;

  // Loadable implementation
  void CancelLoad() override;
  bool IsLoadCanceled() const override;
  bool Load() override;

  // SingleTrackOutputInterface implementation.
  void SetDrmInitData(
      scoped_refptr<const drm::RefCountedDrmInitData> drm_init_data) override;
  void GiveFormat(std::unique_ptr<const MediaFormat> format) override;
  bool WriteSampleData(extractor::ExtractorInputInterface* input,
                       size_t max_length,
                       bool allow_end_of_input,
                       int64_t* bytes_appended) override;
  void WriteSampleData(const void* data, size_t length) override;
  bool WriteSampleDataFixThis(const void* src,
                              size_t length,
                              bool allow_end_of_input,
                              int64_t* num_bytes_written) override;
  void WriteSampleMetadata(int64_t time_us,
                           int64_t duration_us,
                           int32_t flags,
                           size_t size,
                           size_t offset,
                           const std::string* encryption_key_id,
                           const std::string* ivs,
                           std::vector<int32_t>* num_bytes_clear,
                           std::vector<int32_t>* num_bytes_enc) override;

  // SetSeekMap is required for SingleTrackOutputInterface but does nothing
  void GiveSeekMap(
      std::unique_ptr<const extractor::SeekMapInterface> seek_map) override;

 private:
  // Only accessed by loader thread, so no locking required
  upstream::DataSourceInterface* data_source_;
  scoped_refptr<ChunkExtractorWrapper> extractor_wrapper_;

  // No locking required with these because they're const
  const base::TimeDelta sample_offset_;

  // Not accessed by loader thread, so no locking required
  std::unique_ptr<const MediaFormat> media_format_;
  scoped_refptr<const drm::RefCountedDrmInitData> drm_init_data_;

  // Communication in/out of loader thread. Requires lock.
  mutable base::Lock lock_;
  int64_t bytes_loaded_ = 0;
  base::CancellationFlag load_canceled_;

  static std::unique_ptr<const MediaFormat> GetAdjustedMediaFormat(
      std::unique_ptr<const MediaFormat>,
      base::TimeDelta sample_offset);
};

}  // namespace chunk
}  // namespace ndash

#endif  // NDASH_CHUNK_CONTAINER_MEDIA_CHUNK_H_
