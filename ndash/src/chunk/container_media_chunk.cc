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

#include "chunk/container_media_chunk.h"

#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "chunk/chunk_extractor_wrapper.h"
#include "drm/drm_init_data.h"
#include "extractor/extractor.h"
#include "extractor/unbuffered_extractor_input.h"
#include "media_format.h"
#include "upstream/data_source.h"
#include "upstream/data_spec.h"
#include "util/format.h"

namespace ndash {
namespace chunk {

ContainerMediaChunk::ContainerMediaChunk(
    upstream::DataSourceInterface* data_source,
    const upstream::DataSpec* data_spec,
    TriggerReason trigger,
    const util::Format* format,
    int64_t start_time_us,
    int64_t end_time_us,
    int32_t chunk_index,
    base::TimeDelta sample_offset,
    ChunkExtractorWrapper* extractor_wrapper,
    const MediaFormat* media_format,
    scoped_refptr<const drm::RefCountedDrmInitData> drm_init_data,
    bool is_media_format_final,
    int parent_id)
    : BaseMediaChunk(data_spec,
                     trigger,
                     format,
                     start_time_us,
                     end_time_us,
                     chunk_index,
                     is_media_format_final,
                     parent_id),
      data_source_(data_source),
      extractor_wrapper_(extractor_wrapper),
      sample_offset_(sample_offset),
      media_format_(GetAdjustedMediaFormat(media_format, sample_offset)),
      drm_init_data_(std::move(drm_init_data)) {}

ContainerMediaChunk::~ContainerMediaChunk() {}

const MediaFormat* ContainerMediaChunk::GetMediaFormat() const {
  return media_format_.get();
}

scoped_refptr<const drm::RefCountedDrmInitData>
ContainerMediaChunk::GetDrmInitData() const {
  return drm_init_data_;
}

int64_t ContainerMediaChunk::GetNumBytesLoaded() const {
  base::AutoLock auto_lock(lock_);
  return bytes_loaded_;
}

void ContainerMediaChunk::CancelLoad() {
  load_canceled_.Set();
}

bool ContainerMediaChunk::IsLoadCanceled() const {
  return load_canceled_.IsSet();
}

bool ContainerMediaChunk::Load() {
  VLOG(5) << __FUNCTION__;

  upstream::DataSpec load_data_spec(
      upstream::DataSpec::GetRemainder(*data_spec(), bytes_loaded_));

  ssize_t open_size = data_source_->Open(load_data_spec, &load_canceled_);
  if (load_canceled_.IsSet()) {
    LOG(INFO) << "Chunk " << format()->GetMimeType() << " ["
              << base::TimeDelta::FromMicroseconds(start_time_us()) << "-"
              << base::TimeDelta::FromMicroseconds(end_time_us()) << "] "
              << "Canceled " << load_data_spec.DebugString();
    data_source_->Close();
    return false;
  } else if (open_size == upstream::RESULT_IO_ERROR) {
    LOG(INFO) << "Chunk " << format()->GetMimeType() << " ["
              << base::TimeDelta::FromMicroseconds(start_time_us()) << "-"
              << base::TimeDelta::FromMicroseconds(end_time_us()) << "] "
              << "Failed to open " << load_data_spec.DebugString();
    data_source_->Close();
    return false;
  }

  DVLOG(6) << "Open size " << open_size;

  // TODO(adewhurst): DefaultTrackOutput used DataSource directly; potentially
  //                  allow that and skip the ExtractorInput here.
  extractor::UnbufferedExtractorInput extractor_input(
      data_source_, load_data_spec.absolute_stream_position, open_size);

  // Unprotected read here is OK: this is the only thread that writes to
  // bytes_loaded_
  if (bytes_loaded_ == 0) {
    VLOG(9) << "Run extractor wrapper init";
    // Set the target to ourselves.
    extractor_wrapper_->Init(this);
  }
  // Load and parse the sample data.
  int result = extractor::ExtractorInterface::RESULT_CONTINUE;
  while (result == extractor::ExtractorInterface::RESULT_CONTINUE) {
    if (load_canceled_.IsSet()) {
      VLOG(4) << "Canceled";
      break;
    }

    result = extractor_wrapper_->Read(&extractor_input);
    VLOG(10) << "Read result " << result;
  }

  {
    base::AutoLock auto_lock(lock_);
    bytes_loaded_ =
        extractor_input.GetPosition() - load_data_spec.absolute_stream_position;
    DVLOG(4) << "Loaded " << bytes_loaded_;
  }

  data_source_->Close();

  VLOG(5) << __FUNCTION__ << " end";

  return (result == extractor::ExtractorInterface::RESULT_END_OF_INPUT);
}

void ContainerMediaChunk::SetDrmInitData(
    scoped_refptr<const drm::RefCountedDrmInitData> drm_init_data) {
  drm_init_data_ = std::move(drm_init_data);
}
void ContainerMediaChunk::GiveFormat(
    std::unique_ptr<const MediaFormat> media_format) {
  media_format_ = GetAdjustedMediaFormat(media_format.get(), sample_offset_);
  if (!format_given_cb_.is_null()) {
    format_given_cb_.Run(media_format_.get());
  }
}

bool ContainerMediaChunk::WriteSampleData(
    extractor::ExtractorInputInterface* input,
    size_t max_length,
    bool allow_end_of_input,
    int64_t* bytes_appended) {
  return output()->WriteSampleData(input, max_length, allow_end_of_input,
                                   bytes_appended);
}

void ContainerMediaChunk::WriteSampleData(const void* data, size_t length) {
  return output()->WriteSampleData(data, length);
}

bool ContainerMediaChunk::WriteSampleDataFixThis(const void* src,
                                                 size_t length,
                                                 bool allow_end_of_input,
                                                 int64_t* num_bytes_written) {
  return output()->WriteSampleDataFixThis(src, length, allow_end_of_input,
                                          num_bytes_written);
}

void ContainerMediaChunk::WriteSampleMetadata(
    int64_t time_us,
    int64_t duration_us,
    int32_t flags,
    size_t size,
    size_t offset,
    const std::string* encryption_key_id,
    const std::string* iv,
    std::vector<int32_t>* num_bytes_clear,
    std::vector<int32_t>* num_bytes_enc) {
  return output()->WriteSampleMetadata(
      time_us + sample_offset_.InMicroseconds(), duration_us, flags, size,
      offset, encryption_key_id, iv, num_bytes_clear, num_bytes_enc);
}

void ContainerMediaChunk::GiveSeekMap(
    std::unique_ptr<const extractor::SeekMapInterface> seek_map) {
  // Do nothing.
}

std::unique_ptr<MediaFormat> ContainerMediaChunk::GetAdjustedMediaFormat(
    const MediaFormat* format,
    base::TimeDelta sample_offset) {
  std::unique_ptr<MediaFormat> out;

  if (format == nullptr) {
    return out;
  }

  if (!sample_offset.is_zero() &&
      format->GetSubsampleOffsetUs() != kOffsetSampleRelative) {
    out = format->CopyWithSubsampleOffsetUs(format->GetSubsampleOffsetUs() +
                                            sample_offset.InMicroseconds());
  } else {
    // Just make a simple copy of the format.
    // TODO(adewhurst): See if we can avoid copying MediaFormat
    out.reset(new MediaFormat(*format));
  }

  return out;
}

}  // namespace chunk
}  // namespace ndash
