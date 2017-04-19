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
#include "info_queue.h"

namespace ndash {

namespace extractor {

InfoQueue::InfoQueue()
    : capacity_(kSampleCapacityIncrement),
      queue_size_(0),
      absolute_read_index_(0),
      relative_read_index_(0),
      relative_write_index_(0) {
  offsets_ = std::unique_ptr<int64_t[]>(new int64_t[capacity_]);
  durations_ = std::unique_ptr<int64_t[]>(new int64_t[capacity_]);
  times_us_ = std::unique_ptr<int64_t[]>(new int64_t[capacity_]);
  flags_ = std::unique_ptr<int32_t[]>(new int32_t[capacity_]);
  sizes_ = std::unique_ptr<int32_t[]>(new int32_t[capacity_]);
  encryption_key_ids_ =
      std::unique_ptr<std::string[]>(new std::string[capacity_]);
  iv_ = std::unique_ptr<std::string[]>(new std::string[capacity_]);
  num_bytes_clear_ = std::unique_ptr<std::vector<int32_t>[]>(
      new std::vector<int32_t>[capacity_]);
  num_bytes_enc_ = std::unique_ptr<std::vector<int32_t>[]>(
      new std::vector<int32_t>[capacity_]);
}

InfoQueue::~InfoQueue() {}

void InfoQueue::Clear() {
  base::AutoLock lock(lock_);
  absolute_read_index_ = 0;
  relative_read_index_ = 0;
  relative_write_index_ = 0;
  queue_size_ = 0;
}

int32_t InfoQueue::GetWriteIndex() const {
  base::AutoLock lock(lock_);
  return GetWriteIndexInternal();
}

int64_t InfoQueue::DiscardUpstreamSamples(int32_t discard_from_index) {
  base::AutoLock lock(lock_);
  int32_t discard_count = GetWriteIndexInternal() - discard_from_index;
  DCHECK(0 <= discard_count && discard_count <= queue_size_);

  if (discard_count == 0) {
    if (absolute_read_index_ == 0) {
      // queue_size_ == absolute_read_index_ == 0, so nothing has been written
      // to the queue.
      return 0;
    }
    int32_t last_write_index =
        (relative_write_index_ == 0 ? capacity_ : relative_write_index_) - 1;
    return offsets_[last_write_index] + sizes_[last_write_index];
  }

  queue_size_ -= discard_count;
  relative_write_index_ =
      (relative_write_index_ + capacity_ - discard_count) % capacity_;
  return offsets_[relative_write_index_];
}

int32_t InfoQueue::GetReadIndex() const {
  base::AutoLock lock(lock_);
  return absolute_read_index_;
}

bool InfoQueue::PeekSample(SampleHolder* holder,
                           SampleExtrasHolder* extras_holder) {
  DCHECK(holder != nullptr);
  DCHECK(extras_holder != nullptr);
  base::AutoLock lock(lock_);
  if (queue_size_ == 0) {
    return false;
  }
  holder->SetTimeUs(times_us_[relative_read_index_]);
  holder->SetPeekSize(sizes_[relative_read_index_]);
  holder->SetFlags(flags_[relative_read_index_]);
  holder->SetDurationUs(durations_[relative_read_index_]);
  extras_holder->SetOffset(offsets_[relative_read_index_]);
  if (holder->IsEncrypted()) {
    extras_holder->SetEncryptionKeyId(
        &encryption_key_ids_[relative_read_index_]);
    extras_holder->SetIV(&iv_[relative_read_index_]);
    extras_holder->SetNumBytesClear(&num_bytes_clear_[relative_read_index_]);
    extras_holder->SetNumBytesEnc(&num_bytes_enc_[relative_read_index_]);
  } else {
    extras_holder->SetEncryptionKeyId(nullptr);
    extras_holder->SetIV(nullptr);
    extras_holder->SetNumBytesClear(nullptr);
    extras_holder->SetNumBytesEnc(nullptr);
  }
  return true;
}

int64_t InfoQueue::MoveToNextSample() {
  base::AutoLock lock(lock_);
  queue_size_--;
  int lastReadIndex = relative_read_index_++;
  absolute_read_index_++;
  if (relative_read_index_ == capacity_) {
    // Wrap around.
    relative_read_index_ = 0;
  }
  return queue_size_ > 0 ? offsets_[relative_read_index_]
                         : (sizes_[lastReadIndex] + offsets_[lastReadIndex]);
}

int64_t InfoQueue::SkipToKeyframeBefore(int64_t time_us) {
  base::AutoLock lock(lock_);

  int32_t last_write_index =
      (relative_write_index_ == 0 ? capacity_ : relative_write_index_) - 1;

  if (queue_size_ == 0 || time_us < times_us_[relative_read_index_]) {
    VLOG(1) << "Skip failed (before queue start): queue_size_ " << queue_size_
            << ", time_us " << time_us << ", read index time "
            << times_us_[relative_read_index_] << ", write index time "
            << times_us_[last_write_index];
    return -1;
  }

  int64_t last_time_us = times_us_[last_write_index];
  if (time_us > last_time_us) {
    VLOG(1) << "Skip failed (after queue end): time " << time_us
            << ", last_time " << last_time_us;
    return -1;
  }

  // TODO: This can be optimized further using binary search, although the fact
  // that the array
  // is cyclic means we'd need to implement the binary search ourselves.
  int32_t sample_count = 0;
  int32_t sample_count_to_key_frame = -1;
  int32_t search_index = relative_read_index_;
  while (search_index != relative_write_index_) {
    if (times_us_[search_index] > time_us) {
      // We've gone too far.
      break;
    } else if ((flags_[search_index] & util::kSampleFlagSync) != 0) {
      // We've found a keyframe, and we're still before the seek position.
      sample_count_to_key_frame = sample_count;
    }
    search_index = (search_index + 1) % capacity_;
    sample_count++;
  }

  if (sample_count_to_key_frame == -1) {
    VLOG(1) << "Skip failed (couldn't find preceding keyframe)";
    return -1;
  }

  queue_size_ -= sample_count_to_key_frame;
  relative_read_index_ =
      (relative_read_index_ + sample_count_to_key_frame) % capacity_;
  absolute_read_index_ += sample_count_to_key_frame;
  VLOG(1) << "Skip success";
  return offsets_[relative_read_index_];
}

void InfoQueue::CommitSample(int64_t time_us,
                             int64_t duration_us,
                             int32_t sample_flags,
                             int64_t offset,
                             int32_t size,
                             const std::string* encryption_key_id,
                             const std::string* iv,
                             std::vector<int32_t>* num_bytes_clear,
                             std::vector<int32_t>* num_bytes_enc) {
  base::AutoLock lock(lock_);
  times_us_[relative_write_index_] = time_us;
  durations_[relative_write_index_] = duration_us;
  offsets_[relative_write_index_] = offset;
  sizes_[relative_write_index_] = size;
  flags_[relative_write_index_] = sample_flags;
  // Take copies when setting our info queue members.
  if (encryption_key_id != nullptr) {
    encryption_key_ids_[relative_write_index_] = *encryption_key_id;
  } else {
    encryption_key_ids_[relative_write_index_].clear();
  }
  if (iv != nullptr) {
    iv_[relative_write_index_] = *iv;
  } else {
    iv_[relative_write_index_].clear();
  }
  if (num_bytes_clear != nullptr) {
    num_bytes_clear_[relative_write_index_] = *num_bytes_clear;
  } else {
    num_bytes_clear_[relative_write_index_].clear();
  }
  if (num_bytes_enc != nullptr) {
    num_bytes_enc_[relative_write_index_] = *num_bytes_enc;
  } else {
    num_bytes_enc_[relative_write_index_].clear();
  }

  // Increment the write index.
  queue_size_++;
  if (queue_size_ == capacity_) {
    // Increase the capacity.
    int new_capacity = capacity_ + kSampleCapacityIncrement;
    std::unique_ptr<int64_t[]> new_offsets(new int64_t[new_capacity]);
    std::unique_ptr<int64_t[]> new_durations(new int64_t[new_capacity]);
    std::unique_ptr<int64_t[]> new_times_us(new int64_t[new_capacity]);
    std::unique_ptr<int32_t[]> new_flags(new int32_t[new_capacity]);
    std::unique_ptr<int32_t[]> new_sizes(new int32_t[new_capacity]);
    std::unique_ptr<std::string[]> new_encryption_key_ids(
        new std::string[new_capacity]);
    std::unique_ptr<std::string[]> new_iv(new std::string[new_capacity]);
    std::unique_ptr<std::vector<int32_t>[]> new_num_bytes_clear(
        new std::vector<int32_t>[new_capacity]);
    std::unique_ptr<std::vector<int32_t>[]> new_num_bytes_enc(
        new std::vector<int32_t>[new_capacity]);

    int before_wrap = capacity_ - relative_read_index_;
    memcpy(new_offsets.get() + 0, offsets_.get() + relative_read_index_,
           before_wrap * sizeof(int64_t));
    memcpy(new_durations.get() + 0, durations_.get() + relative_read_index_,
           before_wrap * sizeof(int64_t));
    memcpy(new_times_us.get() + 0, times_us_.get() + relative_read_index_,
           before_wrap * sizeof(int64_t));
    memcpy(new_flags.get() + 0, flags_.get() + relative_read_index_,
           before_wrap * sizeof(int32_t));
    memcpy(new_sizes.get() + 0, sizes_.get() + relative_read_index_,
           before_wrap * sizeof(int32_t));
    CopyStrings(new_encryption_key_ids.get(), 0, encryption_key_ids_.get(),
                relative_read_index_, before_wrap);
    CopyStrings(new_iv.get(), 0, iv_.get(), relative_read_index_, before_wrap);
    CopyIntVectors(new_num_bytes_clear.get(), 0, num_bytes_clear_.get(),
                   relative_read_index_, before_wrap);
    CopyIntVectors(new_num_bytes_enc.get(), 0, num_bytes_enc_.get(),
                   relative_read_index_, before_wrap);

    int after_wrap = relative_read_index_;
    memcpy(new_offsets.get() + before_wrap, offsets_.get() + 0,
           after_wrap * sizeof(int64_t));
    memcpy(new_durations.get() + before_wrap, durations_.get() + 0,
           after_wrap * sizeof(int64_t));
    memcpy(new_times_us.get() + before_wrap, times_us_.get() + 0,
           after_wrap * sizeof(int64_t));
    memcpy(new_flags.get() + before_wrap, flags_.get() + 0,
           after_wrap * sizeof(int32_t));
    memcpy(new_sizes.get() + before_wrap, sizes_.get() + 0,
           after_wrap * sizeof(int32_t));
    CopyStrings(new_encryption_key_ids.get(), before_wrap,
                encryption_key_ids_.get(), 0, after_wrap);
    CopyStrings(new_iv.get(), before_wrap, iv_.get(), 0, after_wrap);
    CopyIntVectors(new_num_bytes_clear.get(), before_wrap,
                   num_bytes_clear_.get(), 0, after_wrap);
    CopyIntVectors(new_num_bytes_enc.get(), before_wrap, num_bytes_enc_.get(),
                   0, after_wrap);

    offsets_ = std::move(new_offsets);
    durations_ = std::move(new_durations);
    times_us_ = std::move(new_times_us);
    flags_ = std::move(new_flags);
    sizes_ = std::move(new_sizes);
    encryption_key_ids_ = std::move(new_encryption_key_ids);
    iv_ = std::move(new_iv);
    num_bytes_clear_ = std::move(new_num_bytes_clear);
    num_bytes_enc_ = std::move(new_num_bytes_enc);
    relative_read_index_ = 0;
    relative_write_index_ = capacity_;
    queue_size_ = capacity_;
    capacity_ = new_capacity;
  } else {
    relative_write_index_++;
    if (relative_write_index_ == capacity_) {
      // Wrap around.
      relative_write_index_ = 0;
    }
  }
}

int32_t InfoQueue::GetWriteIndexInternal() const {
  lock_.AssertAcquired();
  return absolute_read_index_ + queue_size_;
}

void InfoQueue::CopyStrings(std::string* dest,
                            int32_t dest_pos,
                            std::string* src,
                            int32_t src_pos,
                            int32_t len) {
  for (int i = 0; i < len; i++) {
    dest[dest_pos + i] = src[src_pos + i];
  }
}

void InfoQueue::CopyIntVectors(std::vector<int32_t>* dest,
                               int32_t dest_pos,
                               std::vector<int32_t>* src,
                               int32_t src_pos,
                               int32_t len) {
  for (int i = 0; i < len; i++) {
    dest[dest_pos + i] = src[src_pos + i];
  }
}

SampleExtrasHolder::SampleExtrasHolder()
    : offset_(0),
      encryption_key_id_(nullptr),
      iv_(nullptr),
      num_bytes_clear_(nullptr),
      num_bytes_enc_(nullptr) {}

SampleExtrasHolder::~SampleExtrasHolder() {}

int32_t SampleExtrasHolder::GetOffset() const {
  return offset_;
}

void SampleExtrasHolder::SetOffset(int32_t offset) {
  offset_ = offset;
}

const std::string* SampleExtrasHolder::GetEncryptionKeyId() const {
  return encryption_key_id_;
}

void SampleExtrasHolder::SetEncryptionKeyId(
    const std::string* encryption_key_id) {
  encryption_key_id_ = encryption_key_id;
}

const std::string* SampleExtrasHolder::GetIV() const {
  return iv_;
}

void SampleExtrasHolder::SetIV(const std::string* ivs) {
  iv_ = ivs;
}

const std::vector<int32_t>* SampleExtrasHolder::GetNumBytesClear() const {
  return num_bytes_clear_;
}

void SampleExtrasHolder::SetNumBytesClear(
    const std::vector<int32_t>* num_bytes_clear) {
  num_bytes_clear_ = num_bytes_clear;
}

const std::vector<int32_t>* SampleExtrasHolder::GetNumBytesEnc() const {
  return num_bytes_enc_;
}

void SampleExtrasHolder::SetNumBytesEnc(
    const std::vector<int32_t>* num_bytes_enc) {
  num_bytes_enc_ = num_bytes_enc;
}

}  // namespace extractor

}  // namespace ndash
