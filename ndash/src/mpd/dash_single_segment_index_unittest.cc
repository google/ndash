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

#include "mpd/dash_single_segment_index.h"

#include <string>

#include "gtest/gtest.h"

namespace ndash {

namespace mpd {

TEST(DashSingleSegmentIndexTests, DashSingleSegmentIndexTest) {
  std::string base_uri = "http://somewhere";
  std::unique_ptr<RangedUri> ranged_uri =
      std::unique_ptr<RangedUri>(new RangedUri(&base_uri, "/ref", 0, -1));
  DashSingleSegmentIndex singleSegmentIndex(std::move(ranged_uri));

  EXPECT_EQ(0, singleSegmentIndex.GetSegmentNum(0, 1000));
  EXPECT_EQ(0, singleSegmentIndex.GetTimeUs(0));
  EXPECT_EQ(1000, singleSegmentIndex.GetDurationUs(0, 1000));
  EXPECT_EQ(0, singleSegmentIndex.GetFirstSegmentNum());
  EXPECT_EQ(0, singleSegmentIndex.GetLastSegmentNum(1000));
  EXPECT_EQ(true, singleSegmentIndex.IsExplicit());
}

}  // namespace mpd

}  // namespace ndash
