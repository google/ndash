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

#include "chunk/initialization_chunk.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "chunk/chunk_extractor_wrapper.h"
#include "drm/drm_init_data.h"
#include "extractor/extractor.h"
#include "extractor/extractor_input.h"
#include "extractor/unbuffered_extractor_input.h"
#include "upstream/data_source.h"
#include "upstream/data_spec.h"
#include "util/format.h"

#include <cstdint>

namespace ndash {
namespace chunk {

InitializationChunk::InitializationChunk(
    upstream::DataSourceInterface* data_source,
    const upstream::DataSpec* data_spec,
    TriggerReason trigger,
    const util::Format* format,
    scoped_refptr<ChunkExtractorWrapper> extractor_wrapper,
    int parent_id)
    : Chunk(data_spec, kTypeMediaInitialization, trigger, format, parent_id),
      data_source_(data_source),
      extractor_wrapper_(extractor_wrapper) {
  VLOG(3) << "+InitChunk " << format->GetMimeType();
}

InitializationChunk::~InitializationChunk() {
  VLOG(3) << "-InitChunk" << format()->GetMimeType();
}

int64_t InitializationChunk::GetNumBytesLoaded() const {
  base::AutoLock auto_lock(lock_);
  return bytes_loaded_;
}

void InitializationChunk::GiveSeekMap(
    std::unique_ptr<const extractor::SeekMapInterface> seek_map) {
  seek_map_ = std::move(seek_map);
}

void InitializationChunk::SetDrmInitData(
    scoped_refptr<const drm::RefCountedDrmInitData> drm_init_data) {
  drm_init_data_ = std::move(drm_init_data);
}

void InitializationChunk::GiveFormat(
    std::unique_ptr<const MediaFormat> media_format) {
  media_format_ = std::move(media_format);
  if (!format_given_cb_.is_null()) {
    format_given_cb_.Run(media_format_.get());
  }
}

bool InitializationChunk::WriteSampleData(
    extractor::ExtractorInputInterface* input,
    size_t max_length,
    bool allow_end_of_input,
    int64_t* bytes_appended) {
  LOG(FATAL) << "Unexpected sample data in initialization chunk";
  return false;
}

void InitializationChunk::WriteSampleData(const void* data, size_t length) {
  LOG(FATAL) << "Unexpected sample data in initialization chunk";
}

void InitializationChunk::WriteSampleMetadata(
    int64_t time_us,
    int64_t duration_us,
    int32_t flags,
    size_t size,
    size_t offset,
    const std::string* encryption_key_id,
    const std::string* ivs,
    std::vector<int32_t>* num_bytes_clear,
    std::vector<int32_t>* num_bytes_enc) {
  LOG(FATAL) << "Unexpected sample data in initialization chunk";
}

bool InitializationChunk::WriteSampleDataFixThis(const void* src,
                                                 size_t length,
                                                 bool allow_end_of_input,
                                                 int64_t* num_bytes_written) {
  LOG(FATAL) << "Unexpected sample data in initialization chunk";
  return false;
}

void InitializationChunk::CancelLoad() {
  base::AutoLock auto_lock(lock_);
  load_canceled_ = true;
}

bool InitializationChunk::IsLoadCanceled() const {
  base::AutoLock auto_lock(lock_);
  return load_canceled_;
}

bool InitializationChunk::Load() {
  upstream::DataSpec load_data_spec =
      upstream::DataSpec::GetRemainder(*data_spec(), bytes_loaded_);

  // Create and open the input.
  ssize_t open_size = data_source_->Open(load_data_spec);
  if (open_size == upstream::RESULT_IO_ERROR) {
    LOG(INFO) << "Failed to open " << load_data_spec.DebugString();
    data_source_->Close();
    return false;
  }

  extractor::UnbufferedExtractorInput input(
      data_source_, load_data_spec.absolute_stream_position, open_size);
  // Unprotected read here is OK: this is the only thread that writes to
  // bytes_loaded_
  if (bytes_loaded_ == 0) {
    // Set the target to ourselves.
    extractor_wrapper_->Init(this);
  }
  // Load and parse the initialization data.
  int result = extractor::ExtractorInterface::RESULT_CONTINUE;
  while (result == extractor::ExtractorInterface::RESULT_CONTINUE) {
    {
      base::AutoLock auto_lock(lock_);
      if (load_canceled_)
        break;
    }

    result = extractor_wrapper_->Read(&input);
  }

  {
    base::AutoLock auto_lock(lock_);

    bytes_loaded_ =
        result == extractor::ExtractorInterface::RESULT_IO_ERROR
            ? 0
            : input.GetPosition() - load_data_spec.absolute_stream_position;
  }

  data_source_->Close();

  return (result == extractor::ExtractorInterface::RESULT_END_OF_INPUT);
}

}  // namespace chunk
}  // namespace ndash
