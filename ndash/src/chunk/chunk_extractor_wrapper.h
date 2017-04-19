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

#ifndef NDASH_CHUNK_CHUNK_EXTRACTOR_WRAPPER_H_
#define NDASH_CHUNK_CHUNK_EXTRACTOR_WRAPPER_H_

#include <cstdint>
#include <memory>

#include "extractor/extractor_output.h"
#include "extractor/track_output.h"

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

namespace chunk {

class SingleTrackOutputInterface;

// An Extractor wrapper for loading chunks containing a single track.
// The wrapper allows switching of the SingleTrackOutput that receives parsed
// data.
class ChunkExtractorWrapper : public extractor::ExtractorOutputInterface,
                              public extractor::TrackOutputInterface {
 public:
  // extractor: The Extractor to wrap.
  explicit ChunkExtractorWrapper(
      std::unique_ptr<extractor::ExtractorInterface> extractor);
  ~ChunkExtractorWrapper() override;

  // Initializes the extractor to output to the provided
  // SingleTrackOutputInterface, and configures it to receive data from a new
  // chunk.
  //
  // output: The SingleTrackOutputInterface that will receive the parsed data.
  //         Ownership is not taken; it must continue to live until the
  //         ChunkExtractorWrapper is destroyed or another output is
  //         registered. A null output (the default after construction) is
  //         allowed as long as none of the other methods are called while the
  //         output is null. It is OK to delete this if the output is null.
  void Init(SingleTrackOutputInterface* output);

  // Reads from the provided ExtractorInput.
  //
  // input: The ExtractorInput from which to read.
  // Returns one of RESULT_CONTINUE, RESULT_END_OF_INPUT or RESULT_ERROR.
  int Read(extractor::ExtractorInputInterface* input);

  // ExtractorOutput implementation.
  extractor::TrackOutputInterface* RegisterTrack(int id) override;
  void DoneRegisteringTracks() override;
  void GiveSeekMap(
      std::unique_ptr<const extractor::SeekMapInterface> seek_map) override;
  void SetDrmInitData(
      scoped_refptr<const drm::RefCountedDrmInitData> drm_init_data) override;

  // TrackOutput implementation.
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
                           const std::string* iv,
                           std::vector<int32_t>* num_bytes_clear,
                           std::vector<int32_t>* num_bytes_enc) override;

 private:
  std::unique_ptr<extractor::ExtractorInterface> extractor_;
  bool extractor_initialized_ = false;
  SingleTrackOutputInterface* output_ = nullptr;

  // Accessed only on the loader thread.
  bool seen_track_ = false;
};

}  // namespace chunk
}  // namespace ndash

#endif  // NDASH_CHUNK_CHUNK_EXTRACTOR_WRAPPER_H_
