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


#include "extractor/default_track_output.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "media_format.h"
#include "sample_holder.h"
#include "upstream/allocator_mock.h"
#include "upstream/default_allocator.h"

using ::testing::_;
using ::testing::Return;

namespace ndash {
namespace extractor {

TEST(DefaultTrackOutput, Empty) {
  upstream::MockAllocator allocator;

  EXPECT_CALL(allocator, GetIndividualAllocationLength())
      .WillRepeatedly(Return(1024));
  EXPECT_CALL(allocator, Allocate()).WillRepeatedly(Return(nullptr));

  DefaultTrackOutput track_output(&allocator);

  // Empty.
  EXPECT_EQ(0, track_output.GetWriteIndex());
  EXPECT_EQ(0, track_output.GetReadIndex());
  EXPECT_FALSE(track_output.HasFormat());
  EXPECT_EQ(nullptr, track_output.GetFormat());
  EXPECT_TRUE(track_output.IsEmpty());
  SampleHolder sample_holder(false);
  EXPECT_FALSE(track_output.GetSample(&sample_holder));

  // Can hold a format.
  std::unique_ptr<MediaFormat> media_format =
      MediaFormat::CreateVideoFormat("1", "video/mp4", "h264", 2200000, 32768,
                                     1234567, 640, 480, nullptr, 16, 45, 1.666);

  track_output.GiveFormat(std::move(media_format));
  EXPECT_TRUE(track_output.HasFormat());
}

TEST(DefaultTrackOutput, SimpleWriteThenRead) {
  upstream::DefaultAllocator allocator(1024, 10);
  DefaultTrackOutput track_output(&allocator);

  char data[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

  // Write 8 bytes.
  int64_t num_appended;
  EXPECT_TRUE(
      track_output.WriteSampleDataFixThis(&data[0], 8, true, &num_appended));
  EXPECT_EQ(8, num_appended);
  track_output.WriteSampleMetadata(0, 33, util::kSampleFlagSync, 8, 0);

  // Write 4 more bytes as another sample
  EXPECT_TRUE(
      track_output.WriteSampleDataFixThis(&data[8], 4, true, &num_appended));
  EXPECT_EQ(4, num_appended);
  track_output.WriteSampleMetadata(100, 33, util::kSampleFlagSync, 4, 0);

  // We have written 2 sample so far.
  EXPECT_EQ(0, track_output.GetReadIndex());
  EXPECT_EQ(2, track_output.GetWriteIndex());

  SampleHolder sample_holder(true);

  // Read the first sample back.
  EXPECT_TRUE(track_output.GetSample(&sample_holder));

  // Data got copied into sample holder.
  EXPECT_EQ(8, sample_holder.GetWrittenSize());
  uint8_t* buf = (uint8_t*)sample_holder.GetBuffer();
  for (int i = 0; i < 8; i++) {
    EXPECT_EQ(i, buf[i]);
  }

  EXPECT_EQ(1, track_output.GetReadIndex());
  EXPECT_EQ(2, track_output.GetWriteIndex());

  // Read the second sample back.
  sample_holder.ClearData();
  EXPECT_TRUE(track_output.GetSample(&sample_holder));

  // Data got copied into sample holder.
  EXPECT_EQ(4, sample_holder.GetWrittenSize());
  buf = (uint8_t*)sample_holder.GetBuffer();
  for (int i = 8; i < 12; i++) {
    EXPECT_EQ(i, buf[i - 8]);
  }

  // Check heads.
  EXPECT_EQ(2, track_output.GetReadIndex());
  EXPECT_EQ(2, track_output.GetWriteIndex());
}

TEST(DefaultTrackOutput, AdvancePastNonKeyFrame) {
  upstream::DefaultAllocator allocator(1024, 10);
  DefaultTrackOutput track_output(&allocator);

  char data[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

  // Write 8 bytes, non key-frame.
  int64_t num_appended;

  EXPECT_TRUE(
      track_output.WriteSampleDataFixThis(&data[0], 8, true, &num_appended));
  EXPECT_EQ(8, num_appended);
  track_output.WriteSampleMetadata(0, 33, 0, 8, 0);

  // Write 4 more bytes as another key frame.
  EXPECT_TRUE(
      track_output.WriteSampleDataFixThis(&data[8], 4, true, &num_appended));
  EXPECT_EQ(4, num_appended);
  track_output.WriteSampleMetadata(100, 33, util::kSampleFlagSync, 4, 0);

  // We have written 2 sample so far.
  EXPECT_EQ(0, track_output.GetReadIndex());
  EXPECT_EQ(2, track_output.GetWriteIndex());

  SampleHolder sample_holder(true);

  // Read the first sample back.
  EXPECT_TRUE(track_output.GetSample(&sample_holder));

  // We should have skipped past the first sample and got the 2nd.
  EXPECT_EQ(4, sample_holder.GetWrittenSize());
  uint8_t* buf = (uint8_t*)sample_holder.GetBuffer();
  for (int i = 8; i < 12; i++) {
    EXPECT_EQ(i, buf[i - 8]);
  }

  EXPECT_EQ(2, track_output.GetReadIndex());
  EXPECT_EQ(2, track_output.GetWriteIndex());
}

TEST(DefaultTrackOutput, DiscardUntil) {
  upstream::DefaultAllocator allocator(1024, 10);
  DefaultTrackOutput track_output(&allocator);

  char data[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

  // Write 4 byte samples 4 times.
  int64_t num_appended;

  int64_t t = 0;
  for (int i = 0; i < 4; i++) {
    EXPECT_TRUE(track_output.WriteSampleDataFixThis(&data[i * 4], 4, true,
                                                    &num_appended));
    EXPECT_EQ(4, num_appended);
    track_output.WriteSampleMetadata(t, 33, util::kSampleFlagSync, 4, 0);
    t = t + 33;
  }

  EXPECT_EQ(99, track_output.GetLargestParsedTimestampUs());

  // We have written 4 samples.
  EXPECT_EQ(0, track_output.GetReadIndex());
  EXPECT_EQ(4, track_output.GetWriteIndex());

  // Discard until time 66.
  track_output.DiscardUntil(66);

  EXPECT_EQ(99, track_output.GetLargestParsedTimestampUs());

  SampleHolder sample_holder(true);

  // Read a sample back.
  EXPECT_TRUE(track_output.GetSample(&sample_holder));

  // We should see 8,9,10,11
  EXPECT_EQ(4, sample_holder.GetWrittenSize());
  uint8_t* buf = (uint8_t*)sample_holder.GetBuffer();
  for (int i = 8; i < 12; i++) {
    EXPECT_EQ(i, buf[i - 8]);
  }
}

TEST(DefaultTrackOutput, DiscardUpstream) {
  upstream::DefaultAllocator allocator(1024, 10);
  DefaultTrackOutput track_output(&allocator);

  char data[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

  // Write 4 byte samples 4 times.
  int64_t num_appended;

  int64_t t = 0;
  for (int i = 0; i < 4; i++) {
    EXPECT_TRUE(track_output.WriteSampleDataFixThis(&data[i * 4], 4, true,
                                                    &num_appended));
    EXPECT_EQ(4, num_appended);
    track_output.WriteSampleMetadata(t, 33, util::kSampleFlagSync, 4, 0);
    t = t + 33;
  }

  EXPECT_EQ(99, track_output.GetLargestParsedTimestampUs());

  // We have written 4 samples.
  EXPECT_EQ(0, track_output.GetReadIndex());
  EXPECT_EQ(4, track_output.GetWriteIndex());

  // Discard last 2 samples.
  track_output.DiscardUpstreamSamples(2);

  // TODO(rmrossi): DiscardUpstreamSamples does not reset the
  // largest time stamp properly.  It takes the value from the read end of
  // the queue when it should be taking it from the write end.  Opened
  // b/31341221 to fix this. Remove the comment when fixed.
  // EXPECT_EQ(33, track_output.GetLargestParsedTimestampUs());

  EXPECT_EQ(0, track_output.GetReadIndex());
  EXPECT_EQ(2, track_output.GetWriteIndex());

  SampleHolder sample_holder(true);

  // Read a sample back.
  EXPECT_TRUE(track_output.GetSample(&sample_holder));

  // We should see 0,1,2,3
  EXPECT_EQ(4, sample_holder.GetWrittenSize());
  uint8_t* buf = (uint8_t*)sample_holder.GetBuffer();
  for (int i = 0; i < 4; i++) {
    EXPECT_EQ(i, buf[i]);
  }

  EXPECT_EQ(1, track_output.GetReadIndex());
  EXPECT_EQ(2, track_output.GetWriteIndex());

  // Read the next sample back.
  sample_holder.ClearData();
  EXPECT_TRUE(track_output.GetSample(&sample_holder));

  // We should see 4,5,6,7
  EXPECT_EQ(4, sample_holder.GetWrittenSize());
  buf = (uint8_t*)sample_holder.GetBuffer();
  for (int i = 4; i < 8; i++) {
    EXPECT_EQ(i, buf[i - 4]);
  }

  EXPECT_EQ(2, track_output.GetReadIndex());
  EXPECT_EQ(2, track_output.GetWriteIndex());
  EXPECT_TRUE(track_output.IsEmpty());
}

}  // namespace extractor
}  // namespace ndash
