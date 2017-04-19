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

#include "crypto_info.h"

#include "gtest/gtest.h"

namespace ndash {

TEST(CryptoInfoTests, CryptoInfoTest) {
  CryptoInfo crypto_info;

  int32_t num_samples = 10;
  std::vector<int32_t> num_clear_bytes;
  std::vector<int32_t> num_encr_bytes;
  std::string key = "key_id";
  std::vector<char> iv;
  iv.reserve(64);

  for (int i = 0; i < num_samples; i++) {
    num_clear_bytes.push_back(i);
    num_encr_bytes.push_back(num_samples - i);
  }

  ASSERT_TRUE(crypto_info.MutableNumBytesClear() != nullptr);
  ASSERT_TRUE(crypto_info.MutableNumBytesEncrypted() != nullptr);
  ASSERT_TRUE(crypto_info.MutableIv() != nullptr);

  crypto_info.SetNumSubSamples(num_samples);
  *crypto_info.MutableNumBytesClear() = num_clear_bytes;
  *crypto_info.MutableNumBytesEncrypted() = num_encr_bytes;
  *crypto_info.MutableKey() = key;
  *crypto_info.MutableIv() = iv;

  EXPECT_EQ(10, crypto_info.GetNumSubSamples());
  EXPECT_EQ("key_id", crypto_info.GetKey());
  EXPECT_EQ(0, crypto_info.GetNumBytesClear().at(0));
  EXPECT_EQ(1, crypto_info.GetNumBytesClear().at(1));
  EXPECT_EQ(9, crypto_info.GetNumBytesEncrypted().at(1));
  EXPECT_EQ(8, crypto_info.GetNumBytesEncrypted().at(2));
}

}  // namespace ndash
