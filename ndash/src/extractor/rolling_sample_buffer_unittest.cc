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

#include "gtest/gtest.h"
#include "sample_holder.h"
#include "upstream/data_spec.h"
#include "upstream/default_allocator.h"
#include "upstream/uri.h"

namespace ndash {

namespace extractor {

class FakeDataSource : public upstream::DataSourceInterface {
 public:
  FakeDataSource() : pos_(0), next_data_(nullptr) {}
  ssize_t Open(const upstream::DataSpec& data_spec,
               const base::CancellationFlag* cancel) override {
    return 0;
  }
  void Close() override {}
  ssize_t Read(void* buffer, size_t read_length) override {
    // Note: No checks here for reading past available data.
    memcpy(buffer, next_data_ + pos_, read_length);
    pos_ += read_length;
    return read_length;
  }
  void SetNextReadSrc(const char* data) {
    next_data_ = data;
    pos_ = 0;
  }

 private:
  int pos_ = 0;
  const char* next_data_;
};

TEST(RollingSampleBufferTests, Empty) {
  upstream::DefaultAllocator allocator(1024, 10);
  RollingSampleBuffer queue(&allocator);

  SampleHolder sample_holder(true);

  EXPECT_EQ(0, queue.GetReadIndex());
  EXPECT_EQ(0, queue.GetWriteIndex());
  EXPECT_FALSE(queue.PeekSample(&sample_holder));
  EXPECT_FALSE(queue.ReadSample(&sample_holder));
}

TEST(RollingSampleBufferTests, SimpleWriteThenRead) {
  upstream::DefaultAllocator allocator(1024, 10);
  RollingSampleBuffer queue(&allocator);

  upstream::Uri fakeUri("nowhere");
  upstream::DataSpec fakeDataSpec(fakeUri);

  static const char data_[] = {0, 1, 2,  3,  4,  5,  6,  7,
                               8, 9, 10, 11, 12, 13, 14, 15};

  SampleHolder sample_holder(true);
  FakeDataSource data_src;
  data_src.SetNextReadSrc(data_);
  int pos = 0;

  // Write 8 bytes.
  int64_t num_appended;
  EXPECT_TRUE(queue.AppendData(&data_src, 8, true, &num_appended));
  EXPECT_EQ(8, num_appended);
  queue.CommitSample(0, 0, 0, pos, 8);
  pos += 8;

  // Data should be available for peek.
  EXPECT_TRUE(queue.PeekSample(&sample_holder));
  EXPECT_EQ(8, sample_holder.GetPeekSize());

  // Write 4 more bytes as another sample
  EXPECT_TRUE(queue.AppendData(&data_src, 4, true, &num_appended));
  EXPECT_EQ(4, num_appended);
  queue.CommitSample(1, 0, 0, pos, 4);
  pos += 4;

  // We have written 12 bytes so far.
  EXPECT_EQ(12, queue.GetWritePosition());

  EXPECT_EQ(0, queue.GetReadIndex());
  EXPECT_EQ(2, queue.GetWriteIndex());

  // Read the first sample back.
  EXPECT_TRUE(queue.ReadSample(&sample_holder));

  // Data got copied into sample holder.
  EXPECT_EQ(8, sample_holder.GetWrittenSize());
  for (int i = 0; i < 8; i++) {
    uint8_t* buf = (uint8_t*)sample_holder.GetBuffer();
    EXPECT_TRUE(buf[i] == i);
  }

  sample_holder.ClearData();

  // Read the next sample back.
  EXPECT_TRUE(queue.ReadSample(&sample_holder));
  // Data got copied into sample holder.
  EXPECT_EQ(4, sample_holder.GetWrittenSize());
  for (int i = 0; i < 4; i++) {
    uint8_t* buf = (uint8_t*)sample_holder.GetBuffer();
    EXPECT_TRUE(buf[i] == i + 8);
  }

  EXPECT_EQ(2, queue.GetReadIndex());
  EXPECT_EQ(2, queue.GetWriteIndex());
}

TEST(RollingSampleBufferTests, EncryptedSample) {
  upstream::DefaultAllocator allocator(1024, 10);
  RollingSampleBuffer queue(&allocator);

  upstream::Uri fakeUri("nowhere");
  upstream::DataSpec fakeDataSpec(fakeUri);

  static const char data_[] = {
      0x08,  // 8 byte iv len, 1 subsample
      0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
      0x08,  // iv
      0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
      0x11,  // sample data
  };

  SampleHolder sample_holder(true);
  FakeDataSource data_src;
  data_src.SetNextReadSrc(data_);

  // Write all encryption bytes.
  int64_t num_appended;
  EXPECT_TRUE(queue.AppendData(&data_src, 17, true, &num_appended));
  EXPECT_EQ(17, num_appended);
  std::string key_id = "key1";
  queue.CommitSample(0, 0, util::kSampleFlagEncrypted, 0, 17, &key_id);

  EXPECT_EQ(0, queue.GetReadIndex());
  EXPECT_EQ(1, queue.GetWriteIndex());

  // Read the sample back, should trigger encryption read and populate
  // crypto info.

  EXPECT_TRUE(queue.ReadSample(&sample_holder));

  // Check extracted crypto info is correct
  EXPECT_EQ(1, sample_holder.GetCryptoInfo()->GetNumSubSamples());
  EXPECT_EQ("key1", sample_holder.GetCryptoInfo()->GetKey());
  for (int i = 1; i <= 8; i++) {
    EXPECT_EQ(i, sample_holder.GetCryptoInfo()->GetIv().at(i - 1));
  }
  // Make sure sample data has just the sample data, not encryption info.
  EXPECT_EQ(8, sample_holder.GetWrittenSize());
  for (int i = 0; i < 8; i++) {
    uint8_t* buf = (uint8_t*)sample_holder.GetBuffer();
    EXPECT_EQ(0x0a + i, buf[i]);
  }
}

TEST(RollingSampleBufferTests, WritePastAllocationSize) {
  upstream::DefaultAllocator allocator(65536, 120);
  RollingSampleBuffer queue(&allocator);

  upstream::Uri fakeUri("nowhere");
  upstream::DataSpec fakeDataSpec(fakeUri);

  SampleHolder sample_holder(true);
  FakeDataSource data_src;
  int pos = 0;

  // Sums to 65983.  Last one will be partially written.
  int sizes[] = {25890, 7830, 3961, 10299, 4412, 8433, 2742, 2416};

  int remain = 65536;
  int partial = -1;
  int64_t num_appended;
  for (int n = 0; n < 8; n++) {
    std::unique_ptr<char[]> data(new char[sizes[n]]);
    memset(data.get(), 0, sizes[n]);
    data_src.SetNextReadSrc(data.get());
    EXPECT_TRUE(queue.AppendData(&data_src, sizes[n], true, &num_appended));
    if (n < 7) {
      EXPECT_EQ(sizes[n], num_appended);
      queue.CommitSample(0, 0, 0, pos, num_appended);
    } else {
      EXPECT_EQ(remain, num_appended);
      queue.CommitSample(0, 0, 0, pos, num_appended);
      partial = num_appended;
    }
    remain -= num_appended;
    pos += num_appended;
  }
  EXPECT_EQ(0, remain);

  // Read it back.
  for (int n = 0; n < 8; n++) {
    sample_holder.ClearData();
    EXPECT_TRUE(queue.ReadSample(&sample_holder));
    if (n < 7) {
      EXPECT_EQ(sizes[n], sample_holder.GetWrittenSize());
    } else {
      EXPECT_EQ(partial, sample_holder.GetWrittenSize());
    }
  }
}

TEST(RollingSampleBufferTests, DropUpstream) {
  // TODO(rmrossi)
}

TEST(RollingSampleBufferTests, DropDownstream) {
  // TODO(rmrossi)
}

}  // namespace extractor

}  // namespace ndash
