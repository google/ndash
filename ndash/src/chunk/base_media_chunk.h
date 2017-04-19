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

#ifndef NDASH_CHUNK_BASE_MEDIA_CHUNK_H_
#define NDASH_CHUNK_BASE_MEDIA_CHUNK_H_

#include <cstdint>
#include <memory>

#include "base/memory/ref_counted.h"
#include "chunk/media_chunk.h"

namespace ndash {

class MediaFormat;

namespace drm {
class RefCountedDrmInitData;
}  // namespace drm

namespace extractor {
class DefaultTrackOutput;
class IndexedTrackOutputInterface;
class TrackOutputInterface;
}  // namespace extractor

namespace upstream {
class DataSpec;
}  // namespace upstream

namespace util {
class Format;
}  // namespace util

namespace chunk {

// A base implementation of MediaChunk, for chunks that contain a single track.
// Loaded samples are output to a DefaultTrackOutput.
// TODO(rmrossi): Merge MediaChunk and BaseMediaChunk into one class. Remove
// casting from MediaChunk to BaseMediaChunk elsewhere.
class BaseMediaChunk : public MediaChunk {
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
  // is_media_format_final: True if GetMediaFormat() and GetDrmInitData() can
  //                        be called at any time to obtain the media format
  //                        and drm initialization data. False if these methods
  //                        are only guaranteed to return correct data after
  //                        the first sample data has been output from the
  //                        chunk.
  // parent_id: Identifier for a parent from which this chunk originates.
  BaseMediaChunk(const upstream::DataSpec* data_spec,
                 TriggerReason trigger,
                 const util::Format* format,
                 int64_t start_time_us,
                 int64_t end_time_us,
                 int32_t chunk_index,
                 bool is_media_format_final,
                 int parent_id);
  ~BaseMediaChunk() override;

  // Initializes the chunk for loading, setting the TrackOutputInterface that
  // will receive samples as they are loaded.
  // output: The output that will receive the loaded samples. It must remain
  //         valid until samples are done being loaded.
  void Init(extractor::IndexedTrackOutputInterface* output);

  // Gets the MediaFormat corresponding to the chunk.
  // See is_media_format_final_ for information about when this method is
  // guaranteed to return correct data.
  // Returns the MediaFormat corresponding to this chunk.
  virtual const MediaFormat* GetMediaFormat() const = 0;

  // Gets the DrmInitData corresponding to the chunk.
  // See is_media_format_final_ for information about when this method is
  // guaranteed to return correct data.
  // Returns the DrmInitData corresponding to this chunk.
  virtual scoped_refptr<const drm::RefCountedDrmInitData> GetDrmInitData()
      const = 0;

  // Accessors
  int first_sample_index() const { return first_sample_index_; }
  bool is_media_format_final() const { return is_media_format_final_; }
  extractor::TrackOutputInterface* output() { return output_; }

 private:
  // Not valid until Init() has been called with a non-null output.
  extractor::TrackOutputInterface* output_ = nullptr;

  // Returns the index of the first sample in the output that was passed to
  // Init() that will originate from this chunk. Not valid until Init() has
  // been called with a non-null output.
  int32_t first_sample_index_ = -1;

  // Whether GetMediaFormat() and GetDrmInitData() can be called at any time to
  // obtain the chunk's media format and drm initialization data. If false,
  // these methods are only guaranteed to return correct data after the first
  // sample data has been output from the chunk.
  bool is_media_format_final_;
};

}  // namespace chunk
}  // namespace ndash

#endif  // NDASH_CHUNK_BASE_MEDIA_CHUNK_H_
