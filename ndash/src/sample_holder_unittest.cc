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

#include "sample_holder.h"

#include "gtest/gtest.h"

namespace ndash {

// Helper macro to convert a string to uint8_t for testing purposes.
#define STR_TO_SAMPLE(str) ((uint8_t*)str.c_str())

TEST(SampleHolderTests, SampleHolderTestAllowExpansion) {
  SampleHolder sample_holder(true);

  EXPECT_TRUE(sample_holder.GetBuffer() == nullptr);
  EXPECT_TRUE(sample_holder.EnsureSpaceForWrite(16));
  EXPECT_TRUE(sample_holder.GetBuffer() != nullptr);

  std::string msg("Hello world");
  EXPECT_TRUE(sample_holder.Write(STR_TO_SAMPLE(msg), msg.size()));
  EXPECT_EQ(msg.size(), sample_holder.GetWrittenSize());

  std::string msg2("Not enough room");
  EXPECT_FALSE(sample_holder.Write(STR_TO_SAMPLE(msg2), msg2.size()));

  // Expand
  EXPECT_TRUE(sample_holder.EnsureSpaceForWrite(msg2.size()));
  EXPECT_TRUE(sample_holder.Write(STR_TO_SAMPLE(msg2), msg2.size()));

  // Clear
  sample_holder.ClearData();
  EXPECT_EQ(0, sample_holder.GetWrittenSize());
}

TEST(SampleHolderTests, SampleHolderTestDisallowExpansion) {
  SampleHolder sample_holder(false);

  std::unique_ptr<uint8_t[]> buf(new uint8_t[16]());
  sample_holder.SetDataBuffer(std::move(buf), 16);

  EXPECT_TRUE(sample_holder.GetBuffer() != nullptr);
  EXPECT_TRUE(sample_holder.EnsureSpaceForWrite(16));
  std::string msg("Hello world");
  EXPECT_TRUE(sample_holder.Write(STR_TO_SAMPLE(msg), msg.size()));
  EXPECT_EQ(msg.size(), sample_holder.GetWrittenSize());

  std::string msg2("Not enough room");
  EXPECT_FALSE(sample_holder.Write(STR_TO_SAMPLE(msg2), msg2.size()));

  // Expand
  EXPECT_FALSE(sample_holder.EnsureSpaceForWrite(msg2.size()));
}

TEST(SampleHolderTests, SampleHolderTestFlags) {
  SampleHolder sh1(true);
  EXPECT_FALSE(sh1.IsDecodeOnly());
  sh1.SetFlags(sh1.GetFlags() | util::kSampleFlagDecodeOnly);
  EXPECT_TRUE(sh1.IsDecodeOnly());

  SampleHolder sh2(true);
  EXPECT_FALSE(sh2.IsEncrypted());
  sh2.SetFlags(sh2.GetFlags() | util::kSampleFlagEncrypted);
  EXPECT_TRUE(sh2.IsEncrypted());

  SampleHolder sh3(true);
  EXPECT_FALSE(sh3.IsSyncFrame());
  sh3.SetFlags(sh3.GetFlags() | util::kSampleFlagSync);
  EXPECT_TRUE(sh3.IsSyncFrame());
}

TEST(SampleHolderTests, SampleHolderAttributes) {
  SampleHolder sh(true);
  sh.SetTimeUs(123);
  sh.SetPeekSize(8192);
  sh.SetDurationUs(5678);
  EXPECT_EQ(123, sh.GetTimeUs());
  EXPECT_EQ(8192, sh.GetPeekSize());
  EXPECT_EQ(5678, sh.GetDurationUs());
}

}  // namespace ndash
