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

#include "chunk/chunk_extractor_wrapper.h"

#include <cstdint>
#include <memory>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "chunk/single_track_output.h"
#include "drm/drm_init_data.h"
#include "extractor/extractor.h"
#include "extractor/seek_map.h"
#include "media_format.h"

namespace ndash {
namespace chunk {

ChunkExtractorWrapper::ChunkExtractorWrapper(
    std::unique_ptr<extractor::ExtractorInterface> extractor)
    : extractor_(std::move(extractor)) {}

ChunkExtractorWrapper::~ChunkExtractorWrapper() {}

void ChunkExtractorWrapper::Init(SingleTrackOutputInterface* output) {
  output_ = output;
  if (!extractor_initialized_) {
    extractor_->Init(this);
    extractor_initialized_ = true;
  } else {
    extractor_->Seek();
  }
}

int ChunkExtractorWrapper::Read(extractor::ExtractorInputInterface* input) {
  int result = extractor_->Read(input, nullptr);
  DCHECK_NE(result, extractor::ExtractorInterface::RESULT_SEEK);
  return result;
}

extractor::TrackOutputInterface* ChunkExtractorWrapper::RegisterTrack(int id) {
  CHECK(!seen_track_);
  seen_track_ = true;
  return this;
}

void ChunkExtractorWrapper::DoneRegisteringTracks() {
  CHECK(seen_track_);
}

void ChunkExtractorWrapper::GiveSeekMap(
    std::unique_ptr<const extractor::SeekMapInterface> seek_map) {
  output_->GiveSeekMap(std::move(seek_map));
}

void ChunkExtractorWrapper::SetDrmInitData(
    scoped_refptr<const drm::RefCountedDrmInitData> drm_init_data) {
  output_->SetDrmInitData(std::move(drm_init_data));
}

void ChunkExtractorWrapper::GiveFormat(
    std::unique_ptr<const MediaFormat> format) {
  return output_->GiveFormat(std::move(format));
}

bool ChunkExtractorWrapper::WriteSampleData(
    extractor::ExtractorInputInterface* input,
    size_t max_length,
    bool allow_end_of_input,
    int64_t* bytes_appended) {
  return output_->WriteSampleData(input, max_length, allow_end_of_input,
                                  bytes_appended);
}

void ChunkExtractorWrapper::WriteSampleData(const void* data, size_t length) {
  output_->WriteSampleData(data, length);
}

bool ChunkExtractorWrapper::WriteSampleDataFixThis(const void* src,
                                                   size_t length,
                                                   bool allow_end_of_input,
                                                   int64_t* num_bytes_written) {
  return output_->WriteSampleDataFixThis(src, length, allow_end_of_input,
                                         num_bytes_written);
}

void ChunkExtractorWrapper::WriteSampleMetadata(
    int64_t time_us,
    int64_t duration_us,
    int32_t flags,
    size_t size,
    size_t offset,
    const std::string* encryption_key_id,
    const std::string* iv,
    std::vector<int32_t>* num_bytes_clear,
    std::vector<int32_t>* num_bytes_enc) {
  output_->WriteSampleMetadata(time_us, duration_us, flags, size, offset,
                               encryption_key_id, iv, num_bytes_clear,
                               num_bytes_enc);
}

}  // namespace chunk
}  // namespace ndash
