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

#ifndef NDASH_CHUNK_SINGLE_SAMPLE_MEDIA_CHUNK_H_
#define NDASH_CHUNK_SINGLE_SAMPLE_MEDIA_CHUNK_H_

#include <cstdint>

#include "base/memory/ref_counted.h"
#include "base/synchronization/cancellation_flag.h"
#include "base/synchronization/lock.h"
#include "chunk/base_media_chunk.h"

namespace ndash {

class MediaFormat;

namespace drm {
class DrmInitDataInterface;
class RefCountedDrmInitData;
}  // namespace drm

namespace upstream {
class DataSourceInterface;
class DataSpec;
}  // namespace upstream

namespace util {
class Format;
}  // namespace util

namespace chunk {

// A BaseMediaChunk for chunks consisting of a single raw sample.
class SingleSampleMediaChunk : public BaseMediaChunk {
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
  // sample_format: The format of the sample.
  // sample_drm_init_data: The DrmInitDataInterface for the sample. Null if the
  //                       sample is not drm protected.
  // parent_id: Identifier for a parent from which this chunk originates.
  SingleSampleMediaChunk(
      upstream::DataSourceInterface* data_source,
      const upstream::DataSpec* data_spec,
      TriggerReason trigger,
      const util::Format* format,
      int64_t start_time_us,
      int64_t end_time_us,
      int32_t chunk_index,
      const MediaFormat* sample_format,
      scoped_refptr<const drm::RefCountedDrmInitData> sample_drm_init_data,
      int parent_id);
  ~SingleSampleMediaChunk() override;

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

 private:
  upstream::DataSourceInterface* data_source_;
  const MediaFormat* sample_format_;
  scoped_refptr<const drm::RefCountedDrmInitData> sample_drm_init_data_;

  mutable base::Lock lock_;
  int64_t bytes_loaded_ = 0;
  base::CancellationFlag load_canceled_;
};

}  // namespace chunk
}  // namespace ndash

#endif  // NDASH_CHUNK_SINGLE_SAMPLE_MEDIA_CHUNK_H_
