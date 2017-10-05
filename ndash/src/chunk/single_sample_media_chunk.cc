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

#include "chunk/single_sample_media_chunk.h"

#include <limits>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "drm/drm_init_data.h"
#include "extractor/track_output.h"
#include "extractor/unbuffered_extractor_input.h"
#include "upstream/data_source.h"
#include "upstream/data_spec.h"
#include "util/util.h"

namespace ndash {
namespace chunk {

SingleSampleMediaChunk::SingleSampleMediaChunk(
    upstream::DataSourceInterface* data_source,
    const upstream::DataSpec* data_spec,
    TriggerReason trigger,
    const util::Format* format,
    int64_t start_time_us,
    int64_t end_time_us,
    int32_t chunk_index,
    std::unique_ptr<const MediaFormat> sample_format,
    scoped_refptr<const drm::RefCountedDrmInitData> sample_drm_init_data,
    int parent_id)
    : BaseMediaChunk(data_spec,
                     trigger,
                     format,
                     start_time_us,
                     end_time_us,
                     chunk_index,
                     true,
                     parent_id),
      data_source_(data_source),
      sample_format_(std::move(sample_format)),
      sample_drm_init_data_(std::move(sample_drm_init_data)) {}

SingleSampleMediaChunk::~SingleSampleMediaChunk() {}

const MediaFormat* SingleSampleMediaChunk::GetMediaFormat() const {
  return sample_format_.get();
}

scoped_refptr<const drm::RefCountedDrmInitData>
SingleSampleMediaChunk::GetDrmInitData() const {
  return sample_drm_init_data_;
}

int64_t SingleSampleMediaChunk::GetNumBytesLoaded() const {
  base::AutoLock auto_lock(lock_);
  return bytes_loaded_;
}

void SingleSampleMediaChunk::CancelLoad() {
  load_canceled_.Set();
}

bool SingleSampleMediaChunk::IsLoadCanceled() const {
  return load_canceled_.IsSet();
}

bool SingleSampleMediaChunk::Load() {
  upstream::DataSpec load_data_spec(
      upstream::DataSpec::GetRemainder(*data_spec(), bytes_loaded_));

  ssize_t open_size = data_source_->Open(load_data_spec, &load_canceled_);
  if (open_size == upstream::RESULT_IO_ERROR) {
    LOG(INFO) << "Failed to open " << load_data_spec.DebugString();
    data_source_->Close();
    return false;
  }

  // TODO(adewhurst): DefaultTrackOutput used DataSource directly; potentially
  //                  allow that and skip the ExtractorInput here.
  extractor::UnbufferedExtractorInput extractor_input(
      data_source_, load_data_spec.absolute_stream_position, open_size);

  int64_t result = 0;
  bool failed = false;

  while (result != upstream::RESULT_END_OF_INPUT) {
    {
      base::AutoLock auto_lock(lock_);
      bytes_loaded_ += result;
    }

    if (load_canceled_.IsSet()) {
      failed = true;
      break;
    }

    if (!output()->WriteSampleData(&extractor_input,
                                   std::numeric_limits<ssize_t>::max(), true,
                                   &result)) {
      LOG(INFO) << "Failed to read " << load_data_spec.DebugString();
      failed = true;
      break;
    }
  }

  data_source_->Close();

  if (failed)
    return false;

  int64_t sample_size;
  {
    base::AutoLock auto_lock(lock_);
    sample_size = bytes_loaded_;
  }

  // Note: encryption_key_id is supposed to be null as per ExoPlayer upstream
  output()->WriteSampleMetadata(
      start_time_us(), end_time_us() - start_time_us(), util::kSampleFlagSync,
      sample_size, 0, nullptr /* encryption_key_id */);

  return true;
}

}  // namespace chunk
}  // namespace ndash
