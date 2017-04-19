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

#include "mpd/single_segment_base.h"

#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "mpd/mpd_unittest_helper.h"
#include "util/format.h"

namespace ndash {

namespace mpd {

TEST(SingleSegmentBaseTests, SingleSegmentBaseConstructor1) {
  std::unique_ptr<SingleSegmentBase> ssb = CreateTestSingleSegmentBase();

  // Should match constructor args.
  // 90 seconds is 90000000 microseconds.
  ASSERT_TRUE(ssb.get() != nullptr);
  EXPECT_EQ(90000000, ssb->GetPresentationTimeOffsetUs());
  const std::string* uri = ssb->GetUri();
  ASSERT_TRUE(uri != nullptr);
  EXPECT_EQ("http://segmentsource/", *uri);

  // Index should match what we provided.
  std::unique_ptr<RangedUri> index = ssb->GetIndex();
  EXPECT_EQ("http://segmentsource/", index->GetUriString());
  EXPECT_EQ(0, index->GetStart());
  EXPECT_EQ(30000, index->GetLength());
}

TEST(SingleSegmentBaseTests, SingleSegmentBaseConstructor2) {
  std::unique_ptr<std::string> segment_uri(
      new std::string("http://segmentsource/"));
  SingleSegmentBase ssb(std::move(segment_uri));

  // When not specified, presentation time offset defaults to 0.
  EXPECT_EQ(0, ssb.GetPresentationTimeOffsetUs());
  const std::string* uri = ssb.GetUri();
  ASSERT_TRUE(uri != nullptr);
  EXPECT_EQ("http://segmentsource/", *uri);

  // No index was provided, expect null;
  EXPECT_EQ(nullptr, ssb.GetIndex().get());
}

}  // namespace mpd

}  // namespace ndash
