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

#include "extractor/info_queue.h"

#include "gtest/gtest.h"
#include "sample_holder.h"

namespace ndash {

namespace extractor {

TEST(InfoQueueTests, PeekBeforeCommit) {
  InfoQueue info_queue;
  SampleHolder sample_holder(true);
  SampleExtrasHolder extras_holder;
  EXPECT_FALSE(info_queue.PeekSample(&sample_holder, &extras_holder));
}

TEST(InfoQueueTests, CommitAndPeek) {
  InfoQueue info_queue;
  SampleExtrasHolder extras_holder;
  std::string key = "keyId";
  std::string iv = "iv";
  std::vector<int32_t> clear_counts;
  std::vector<int32_t> enc_counts;
  clear_counts.push_back(5);
  clear_counts.push_back(8);
  enc_counts.push_back(12);
  enc_counts.push_back(3);
  int flags = util::kSampleFlagDecodeOnly | util::kSampleFlagEncrypted;
  info_queue.CommitSample(1234, 33, flags, 5, 32768, &key, &iv, &clear_counts,
                          &enc_counts);
  SampleHolder sample_holder(true);
  EXPECT_TRUE(info_queue.PeekSample(&sample_holder, &extras_holder));
  EXPECT_EQ(1234, sample_holder.GetTimeUs());
  EXPECT_EQ(33, sample_holder.GetDurationUs());
  EXPECT_EQ(5, extras_holder.GetOffset());
  EXPECT_EQ(flags, sample_holder.GetFlags());
  EXPECT_EQ(32768, sample_holder.GetPeekSize());
  EXPECT_EQ("keyId", *extras_holder.GetEncryptionKeyId());
  EXPECT_EQ("iv", *extras_holder.GetIV());
  ASSERT_TRUE(extras_holder.GetNumBytesClear()->size() == 2);
  ASSERT_TRUE(extras_holder.GetNumBytesEnc()->size() == 2);
  EXPECT_EQ(5, extras_holder.GetNumBytesClear()->at(0));
  EXPECT_EQ(8, extras_holder.GetNumBytesClear()->at(1));
  EXPECT_EQ(12, extras_holder.GetNumBytesEnc()->at(0));
  EXPECT_EQ(3, extras_holder.GetNumBytesEnc()->at(1));
}

TEST(InfoQueueTests, CommitBeyondInitialCapacity) {
  InfoQueue info_queue;
  SampleExtrasHolder extras_holder;
  for (int i = 0; i < kSampleCapacityIncrement * 2; i++) {
    info_queue.CommitSample(i, i, 0, i, i);
  }
  // We haven't read anything so write index should be how many we committed.
  EXPECT_EQ(kSampleCapacityIncrement * 2, info_queue.GetWriteIndex());

  SampleHolder sample_holder(true);

  // Make sure the expansion kept everything in-tact.
  for (int i = 0; i < kSampleCapacityIncrement * 2; i++) {
    EXPECT_TRUE(info_queue.PeekSample(&sample_holder, &extras_holder));
    EXPECT_EQ(i, sample_holder.GetTimeUs());
    EXPECT_EQ(i, sample_holder.GetDurationUs());
    EXPECT_EQ(i, extras_holder.GetOffset());
    EXPECT_EQ(i, sample_holder.GetPeekSize());
    info_queue.MoveToNextSample();
  }
}

TEST(InfoQueueTests, DiscardFromWriteSide) {
  InfoQueue info_queue;
  int32_t offset = 0;
  for (int i = 0; i < 1024; i++) {
    int32_t size_this_commit = i + 1;
    info_queue.CommitSample(i + 1, 33, 0, offset, size_this_commit);
    offset += size_this_commit;
  }

  // Discard from index 256 onward
  int64_t num_bytes_remain = info_queue.DiscardUpstreamSamples(256);

  // Total bytes written should now be (N*N+1)/2 or (256*257)/2 = 32896
  // as if we only wrote 256 in the commit loop.
  EXPECT_EQ(32896, num_bytes_remain);
}

TEST(InfoQueueTests, Wrapping) {
  InfoQueue info_queue;
  SampleExtrasHolder extras_holder;

  // Fill up the queue.
  int32_t offset = 0;
  int32_t num_written = 0;
  for (int i = 0; i < kSampleCapacityIncrement - 1; i++) {
    int32_t size_this_commit = i + 1;
    info_queue.CommitSample(i + 1, 33, 0, offset, size_this_commit);
    offset += size_this_commit;
    num_written++;
  }

  // Consume some from the read end.
  SampleHolder sample_holder(true);
  int32_t first_needed = 0;
  int32_t num_read = 0;
  for (int i = 0; i < 128; i++) {
    EXPECT_TRUE(info_queue.PeekSample(&sample_holder, &extras_holder));
    first_needed += sample_holder.GetPeekSize();
    EXPECT_EQ(first_needed, info_queue.MoveToNextSample());
    num_read++;
  }

  EXPECT_EQ(128, info_queue.GetReadIndex());

  // Write some more.
  for (int i = 128; i < 255; i++) {
    int32_t size_this_commit = i + 1;
    info_queue.CommitSample(i + 1, 33, 0, offset, size_this_commit);
    offset += size_this_commit;
    num_written++;
  }

  // Buffer is now wrapped internally.

  // Discard the last thing we wrote (exercises special case) in discard.
  info_queue.DiscardUpstreamSamples(num_written);

  // num_written should be the next available write position since we
  // just discarded it.
  EXPECT_EQ(num_written, info_queue.GetWriteIndex());

  // There should be num_written - num_read still in the queue.
  // Consume the remainder to make sure wrapping around works
  for (int i = 0; i < num_written - num_read; i++) {
    EXPECT_TRUE(info_queue.PeekSample(&sample_holder, &extras_holder));
    first_needed += sample_holder.GetPeekSize();
    EXPECT_EQ(first_needed, info_queue.MoveToNextSample());
  }

  // Queue should now be empty
  EXPECT_EQ(info_queue.GetWriteIndex(), info_queue.GetReadIndex());
}

TEST(InfoQueueTests, SkipToKeyframeBefore) {
  InfoQueue info_queue;

  // Fill up the queue.
  int32_t offset = 0;
  int64_t last_time = 0;
  for (int i = 0; i < kSampleCapacityIncrement - 1; i++) {
    // Make every 10th frame a key frame
    int32_t flags = i % 10 == 0 ? util::kSampleFlagSync : 0;
    int64_t time_us = 10000 + i;
    int64_t duration_us = 1000 + i * 1000;
    int32_t size_this_commit = i + 1;
    info_queue.CommitSample(time_us, duration_us, flags, offset,
                            size_this_commit);
    offset += size_this_commit;
    last_time = time_us;
  }

  // Can't skip to key frame for a time we don't have (before data)
  int32_t before_skip_attempt = info_queue.GetReadIndex();
  EXPECT_EQ(-1, info_queue.SkipToKeyframeBefore(5000));
  // Read index should not have moved.
  EXPECT_EQ(before_skip_attempt, info_queue.GetReadIndex());

  // Can't skip to key frame for a time we don't have (after data)
  EXPECT_EQ(-1, info_queue.SkipToKeyframeBefore(last_time + 1));
  // Read index should not have moved.
  EXPECT_EQ(before_skip_attempt, info_queue.GetReadIndex());

  // First time should give us first sample
  EXPECT_EQ(0, info_queue.SkipToKeyframeBefore(10000));
  EXPECT_EQ(0, info_queue.GetReadIndex());

  // Between 10001 - 10009 should still be first sample.
  EXPECT_EQ(0, info_queue.SkipToKeyframeBefore(10001));
  EXPECT_EQ(0, info_queue.GetReadIndex());

  // Between 10010 should still be 2nd sample which should have an offset
  // of 10*11/2 = 55
  EXPECT_EQ(55, info_queue.SkipToKeyframeBefore(10010));
  EXPECT_EQ(10, info_queue.GetReadIndex());

  // Between 10080 should still be 8th sample which should have an offset
  // of 80*81/2 = 3240
  EXPECT_EQ(3240, info_queue.SkipToKeyframeBefore(10080));
  EXPECT_EQ(80, info_queue.GetReadIndex());

  info_queue.Clear();
  EXPECT_EQ(0, info_queue.GetReadIndex());
  EXPECT_EQ(0, info_queue.GetWriteIndex());
}

}  // namespace extractor

}  // namespace ndash
