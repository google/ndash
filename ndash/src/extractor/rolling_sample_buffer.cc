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

#include "extractor/rolling_sample_buffer.h"

#include "upstream/allocator.h"
#include "util/util.h"

namespace {
constexpr int kInitialScratchSize = 32;
}

namespace ndash {

namespace extractor {

typedef upstream::AllocatorInterface::AllocationType* SampleAllocation;

RollingSampleBuffer::RollingSampleBuffer(
    upstream::AllocatorInterface* allocator)
    : allocator_(allocator),
      allocation_length_(allocator->GetIndividualAllocationLength()),
      scratch_position_(0),
      total_bytes_dropped_(0),
      total_bytes_written_(0),
      last_allocation_(nullptr),
      last_allocation_offset_(allocation_length_) {
  scratch_.resize(kInitialScratchSize);
}

RollingSampleBuffer::~RollingSampleBuffer() {}

void RollingSampleBuffer::Clear() {
  base::AutoLock lock(lock_);
  info_queue_.Clear();
  while (!data_queue_.empty()) {
    // TODO(adewhurst): The const_casts are wrong. The const-ness was added to
    //                  help improve clarity between the producer side and the
    //                  consumer side, but this isn't the right final answer.
    //                  More thought needs to go into the API.
    allocator_->Release(const_cast<SampleAllocation>(data_queue_.back()));
    data_queue_.pop_back();
  }
  total_bytes_dropped_ = 0;
  total_bytes_written_ = 0;
  last_allocation_ = nullptr;
  last_allocation_offset_ = allocation_length_;
}

int32_t RollingSampleBuffer::GetWriteIndex() const {
  return info_queue_.GetWriteIndex();
}

void RollingSampleBuffer::DiscardUpstreamSamples(int32_t discard_from_index) {
  base::AutoLock lock(lock_);
  total_bytes_written_ = info_queue_.DiscardUpstreamSamples(discard_from_index);
  DropUpstreamFrom(total_bytes_written_);
}

int32_t RollingSampleBuffer::GetReadIndex() const {
  base::AutoLock lock(lock_);
  return info_queue_.GetReadIndex();
}

bool RollingSampleBuffer::PeekSample(SampleHolder* sample_holder) {
  return info_queue_.PeekSample(sample_holder, &extras_holder_);
}

void RollingSampleBuffer::SkipSample() {
  base::AutoLock lock(lock_);
  int64_t next_offset = info_queue_.MoveToNextSample();
  DropDownstreamTo(next_offset);
}

bool RollingSampleBuffer::SkipToKeyframeBefore(int64_t timeUs) {
  base::AutoLock lock(lock_);
  int64_t next_offset = info_queue_.SkipToKeyframeBefore(timeUs);
  if (next_offset == -1) {
    return false;
  }
  DropDownstreamTo(next_offset);
  return true;
}

bool RollingSampleBuffer::ReadSample(SampleHolder* sample_holder) {
  DCHECK(sample_holder != nullptr);
  base::AutoLock lock(lock_);
  // Write the sample information into the holder and extrasHolder.
  bool have_sample = info_queue_.PeekSample(sample_holder, &extras_holder_);
  if (!have_sample) {
    return false;
  }

  if (sample_holder->IsEncrypted()) {
    if (extras_holder_.GetIV()->size() == 0) {
      // TODO(rmrossi): There's probably a better way of detecting this.
      // Encryption data is part of the data stream. Read it and set the sample
      // holder's crypto info accordingly.
      ReadEncryptionData(sample_holder, &extras_holder_);
    } else {
      // IV and counts already parsed out of the stream. Transfer them to
      // the sample holder's crypto info.
      std::string* key = sample_holder->MutableCryptoInfo()->MutableKey();
      std::vector<char>* iv = sample_holder->MutableCryptoInfo()->MutableIv();
      std::vector<int32_t>* clear_bytes =
          sample_holder->MutableCryptoInfo()->MutableNumBytesClear();
      std::vector<int32_t>* enc_bytes =
          sample_holder->MutableCryptoInfo()->MutableNumBytesEncrypted();

      const std::string* sample_key = extras_holder_.GetEncryptionKeyId();
      const std::string* sample_iv = extras_holder_.GetIV();
      const std::vector<int32_t>* sample_nbc =
          extras_holder_.GetNumBytesClear();
      const std::vector<int32_t>* sample_nbe = extras_holder_.GetNumBytesEnc();

      // TODO(rmrossi): There's no need for CryptoInfo->NumSubSamples since
      // we are using vector.  Get rid of it later.
      DCHECK(sample_nbc->size() == sample_nbe->size());
      sample_holder->MutableCryptoInfo()->SetNumSubSamples(sample_nbc->size());

      // None of the sample values should be null as they are pointers into
      // the storage held by the info queue.
      DCHECK(sample_key != nullptr);
      DCHECK(sample_iv != nullptr);
      DCHECK(sample_nbc != nullptr);
      DCHECK(sample_nbe != nullptr);

      *key = *sample_key;
      std::copy(sample_iv->begin(), sample_iv->end(), std::back_inserter(*iv));
      std::copy(sample_nbc->begin(), sample_nbc->end(),
                std::back_inserter(*clear_bytes));
      std::copy(sample_nbe->begin(), sample_nbe->end(),
                std::back_inserter(*enc_bytes));
    }
  }
  // Write the sample data into the holder.
  sample_holder->EnsureSpaceForWrite(sample_holder->GetPeekSize());
  ReadDataToSampleHolder(extras_holder_.GetOffset(), sample_holder,
                         sample_holder->GetPeekSize());
  // Advance the read head.
  int64_t next_offset = info_queue_.MoveToNextSample();
  DropDownstreamTo(next_offset);
  return true;
}

int64_t RollingSampleBuffer::GetWritePosition() const {
  base::AutoLock lock(lock_);
  return total_bytes_written_;
}

bool RollingSampleBuffer::AppendData(upstream::DataSourceInterface* data_source,
                                     size_t length,
                                     bool allow_end_of_input,
                                     int64_t* num_bytes_appended) {
  base::AutoLock lock(lock_);
  length = PrepareForAppend(length);
  int32_t bytesAppended =
      data_source->Read(last_allocation_ + last_allocation_offset_, length);
  if (bytesAppended == util::kResultEndOfInput) {
    if (allow_end_of_input) {
      return util::kResultEndOfInput;
    }
    return false;
  }
  last_allocation_offset_ += bytesAppended;
  total_bytes_written_ += bytesAppended;
  *num_bytes_appended = bytesAppended;
  return true;
}

void RollingSampleBuffer::AppendData(const void* src, size_t length) {
  base::AutoLock lock(lock_);
  length = PrepareForAppend(length);
  memcpy(last_allocation_ + last_allocation_offset_, src, length);
  last_allocation_offset_ += length;
  total_bytes_written_ += length;
}

bool RollingSampleBuffer::AppendDataFixThis(const void* src,
                                            size_t length,
                                            bool allow_end_of_input,
                                            int64_t* num_bytes_appended) {
  base::AutoLock lock(lock_);
  length = PrepareForAppend(length);
  memcpy(last_allocation_ + last_allocation_offset_, src, length);
  if (length == util::kResultEndOfInput) {
    if (allow_end_of_input) {
      return util::kResultEndOfInput;
    }
    return false;
  }
  last_allocation_offset_ += length;
  total_bytes_written_ += length;
  *num_bytes_appended = length;
  return true;
}

void RollingSampleBuffer::CommitSample(int64_t sample_time_us,
                                       int64_t duration_us,
                                       int32_t flags,
                                       int64_t position,
                                       int32_t size,
                                       const std::string* encryption_key_id,
                                       const std::string* iv,
                                       std::vector<int32_t>* num_bytes_clear,
                                       std::vector<int32_t>* num_bytes_enc) {
  info_queue_.CommitSample(sample_time_us, duration_us, flags, position, size,
                           encryption_key_id, iv, num_bytes_clear,
                           num_bytes_enc);
}

// Private member functions

void RollingSampleBuffer::ReadDataToScratch(int64_t absolute_position,
                                            int32_t length) {
  lock_.AssertAcquired();
  int32_t remaining = length;

  while (remaining > 0) {
    DropDownstreamTo(absolute_position);
    int32_t position_in_allocation =
        (int)(absolute_position - total_bytes_dropped_);
    int32_t to_copy =
        std::min(remaining, allocation_length_ - position_in_allocation);
    const upstream::AllocatorInterface::AllocationType* allocation =
        data_queue_.front();
    char* data = (char*)scratch_.data() + scratch_position_;
    memcpy(data, allocation + position_in_allocation, to_copy);
    scratch_position_ += to_copy;
    absolute_position += to_copy;
    remaining -= to_copy;
  }
}

int32_t RollingSampleBuffer::ReadUnsignedShortFromScratch() {
  lock_.AssertAcquired();
  int32_t v = (scratch_[scratch_position_] & 0xFF) << 8 |
              (scratch_[scratch_position_ + 1] & 0xFF);
  scratch_position_ += 2;
  return v;
}

int32_t RollingSampleBuffer::ReadUnsignedIntToIntFromScratch() {
  lock_.AssertAcquired();
  int32_t v = (scratch_[scratch_position_] & 0xFF) << 24 |
              (scratch_[scratch_position_ + 1] & 0xFF) << 16 |
              (scratch_[scratch_position_ + 2] & 0xFF) << 8 |
              (scratch_[scratch_position_ + 3] & 0xFF);
  scratch_position_ += 4;
  return v;
}

void RollingSampleBuffer::EnsureScratchCapacity(int32_t capacity) {
  lock_.AssertAcquired();
  if (scratch_.capacity() < capacity) {
    scratch_.reserve(capacity);
  }
}

void RollingSampleBuffer::ReadData(int64_t absolute_position,
                                   char* target,
                                   int32_t length) {
  lock_.AssertAcquired();
  int32_t bytes_read = 0;
  while (bytes_read < length) {
    DropDownstreamTo(absolute_position);
    int32_t position_in_allocation =
        (int)(absolute_position - total_bytes_dropped_);
    int32_t to_copy = std::min(length - bytes_read,
                               allocation_length_ - position_in_allocation);
    const upstream::AllocatorInterface::AllocationType* allocation =
        data_queue_.front();
    memcpy(target + bytes_read, allocation + position_in_allocation, to_copy);
    absolute_position += to_copy;
    bytes_read += to_copy;
  }
}

void RollingSampleBuffer::ReadDataToSampleHolder(int64_t absolute_position,
                                                 SampleHolder* target,
                                                 int32_t length) {
  lock_.AssertAcquired();
  int32_t bytes_read = 0;
  while (bytes_read < length) {
    DropDownstreamTo(absolute_position);
    int32_t position_in_allocation =
        (int)(absolute_position - total_bytes_dropped_);
    int32_t to_copy = std::min(length - bytes_read,
                               allocation_length_ - position_in_allocation);
    const upstream::AllocatorInterface::AllocationType* allocation =
        data_queue_.front();
    CHECK(target->Write(allocation + position_in_allocation, to_copy));
    absolute_position += to_copy;
    bytes_read += to_copy;
  }
}

void RollingSampleBuffer::ReadEncryptionData(SampleHolder* sample_holder,
                                             SampleExtrasHolder* extrasHolder) {
  DCHECK(sample_holder != nullptr);
  DCHECK(extrasHolder != nullptr);
  lock_.AssertAcquired();
  int64_t offset = extrasHolder->GetOffset();

  // Read the signal byte.
  ReadDataToScratch(offset, 1);
  offset++;
  char signal_byte = scratch_[0];
  bool subsample_encryption = (signal_byte & 0x80) != 0;
  int32_t iv_size = signal_byte & 0x7F;

  // Read the initialization vector.
  std::vector<char>* iv = sample_holder->MutableCryptoInfo()->MutableIv();
  if (iv->size() < 16) {
    // TODO(adewhurst): Why 16?
    iv->resize(16);
  }

  ReadData(offset, iv->data(), iv_size);
  offset += iv_size;

  // Read the subsample count, if present.
  int32_t subsample_count;
  if (subsample_encryption) {
    ReadDataToScratch(offset, 2);
    offset += 2;
    scratch_position_ = 0;
    subsample_count = ReadUnsignedShortFromScratch();
  } else {
    subsample_count = 1;
  }

  // Write the clear and encrypted subsample sizes.
  std::vector<int32_t>* clear_data_sizes =
      sample_holder->MutableCryptoInfo()->MutableNumBytesClear();
  clear_data_sizes->resize(subsample_count);

  std::vector<int32_t>* encrypted_data_sizes =
      sample_holder->MutableCryptoInfo()->MutableNumBytesEncrypted();
  encrypted_data_sizes->resize(subsample_count);

  if (subsample_encryption) {
    int32_t subsample_data_length = 6 * subsample_count;
    EnsureScratchCapacity(subsample_data_length);
    ReadDataToScratch(offset, subsample_data_length);
    offset += subsample_data_length;
    scratch_position_ = 0;
    for (int i = 0; i < subsample_count; i++) {
      clear_data_sizes->assign(i, ReadUnsignedShortFromScratch());
      encrypted_data_sizes->assign(i, ReadUnsignedIntToIntFromScratch());
    }
  } else {
    clear_data_sizes->assign(0, 0);
    encrypted_data_sizes->assign(0,
                                 sample_holder->GetPeekSize() -
                                     (int)(offset - extrasHolder->GetOffset()));
  }

  // Populate the cryptoInfo.
  sample_holder->MutableCryptoInfo()->SetNumSubSamples(subsample_count);
  *sample_holder->MutableCryptoInfo()->MutableKey() =
      *extrasHolder->GetEncryptionKeyId();

  // Adjust the offset and size to take into account the bytes read.
  int32_t bytes_read = (int32_t)(offset - extrasHolder->GetOffset());
  extrasHolder->SetOffset(extrasHolder->GetOffset() + bytes_read);
  sample_holder->SetPeekSize(sample_holder->GetPeekSize() - bytes_read);
}

void RollingSampleBuffer::DropUpstreamFrom(int64_t absolute_position) {
  lock_.AssertAcquired();
  int32_t relativePosition =
      (int32_t)(absolute_position - total_bytes_dropped_);
  // Calculate the index of the allocation containing the position, and the
  // offset within it.
  int32_t allocation_index = relativePosition / allocation_length_;
  int32_t allocation_offset = relativePosition % allocation_length_;
  // We want to discard any allocations after the one at allocationIdnex.
  int32_t discard_count = data_queue_.size() - allocation_index - 1;
  if (allocation_offset == 0) {
    // If the allocation at allocation_index is empty, we should discard that
    // one too.
    discard_count++;
  }
  // Discard the allocations.
  for (int i = 0; i < discard_count; i++) {
    allocator_->Release(const_cast<SampleAllocation>(data_queue_.back()));
    data_queue_.pop_back();
  }
  // Update last_allocation_ and last_allocation_offset_ to reflect the new
  // position.
  last_allocation_ = const_cast<SampleAllocation>(data_queue_.back());
  last_allocation_offset_ =
      allocation_offset == 0 ? allocation_length_ : allocation_offset;
}

// This method should be called with the lock held.
void RollingSampleBuffer::DropDownstreamTo(int64_t absolute_position) {
  lock_.AssertAcquired();
  int32_t relative_position =
      (int32_t)(absolute_position - total_bytes_dropped_);
  int32_t allocation_index = relative_position / allocation_length_;
  for (int i = 0; i < allocation_index; i++) {
    allocator_->Release(const_cast<SampleAllocation>(data_queue_.front()));
    data_queue_.pop_front();
    total_bytes_dropped_ += allocation_length_;
  }
}

int32_t RollingSampleBuffer::PrepareForAppend(int32_t length) {
  lock_.AssertAcquired();
  if (last_allocation_offset_ == allocation_length_) {
    last_allocation_offset_ = 0;
    upstream::AllocatorInterface::AllocationType* new_allocation =
        allocator_->Allocate();
    last_allocation_ = new_allocation;
    data_queue_.push_back(new_allocation);
  }
  return std::min(length, allocation_length_ - last_allocation_offset_);
}

}  // namespace extractor

}  // namespace ndash
