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

#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "mpd/mpd_unittest_helper.h"
#include "mpd/segment_list.h"
#include "util/format.h"
#include "util/util.h"

namespace ndash {

namespace mpd {

TEST(SegmentListTests, SegmentListTest) {
  std::string content_id = "1234";
  util::Format format = CreateTestFormat();

  std::string init_base_uri = "http://initialization";
  std::string init_reference_uri = "/initialize_me";
  std::unique_ptr<std::string> media_base_uri(new std::string("http://media"));

  std::unique_ptr<RangedUri> initialization(
      new RangedUri(&init_base_uri, init_reference_uri, 0, 10000));

  // Simulate a 7 second period with 3 segments.
  int64_t period_duration = 7000;
  int64_t timescale = 1000;
  int64_t period_duration_us =
      period_duration * util::kMicrosPerSecond / timescale;
  int64_t segment_duration = 2500;
  // 7 seconds leaves last segment with 2 seconds.
  int64_t last_partial_segment_duration = 2000;

  std::unique_ptr<std::vector<RangedUri>> media_list(
      new std::vector<RangedUri>);
  RangedUri seg0(media_base_uri.get(), "/seg0", 0, -1);
  RangedUri seg1(media_base_uri.get(), "/seg1", 0, -1);
  RangedUri seg2(media_base_uri.get(), "/seg2", 0, -1);
  media_list->push_back(seg0);
  media_list->push_back(seg1);
  media_list->push_back(seg2);

  std::unique_ptr<SegmentList> segment_list(new SegmentList(
      std::move(media_base_uri), std::move(initialization), timescale, 0, 0,
      segment_duration,
      std::unique_ptr<std::vector<SegmentTimelineElement>>(nullptr),
      std::move(media_list)));

  EXPECT_EQ(true, segment_list->IsExplicit());

  // First segment should be 2.5 seconds
  EXPECT_EQ(segment_duration * util::kMicrosPerSecond / timescale,
            segment_list->GetSegmentDurationUs(0, period_duration_us));
  // Same for a middle segment
  EXPECT_EQ(segment_duration * util::kMicrosPerSecond / timescale,
            segment_list->GetSegmentDurationUs(1, period_duration_us));
  // Very last segment should be only 2 seconds.
  EXPECT_EQ(last_partial_segment_duration * util::kMicrosPerSecond / timescale,
            segment_list->GetSegmentDurationUs(2, period_duration_us));

  // Test upper/lower bounds.
  EXPECT_EQ(0, segment_list->GetFirstSegmentNum());
  EXPECT_EQ(2, segment_list->GetLastSegmentNum(period_duration_us));

  // 0 seconds in should be 1st segment
  EXPECT_EQ(0, segment_list->GetSegmentNum(0, period_duration_us));
  // 2.5 seconds in should be 2nd segment
  EXPECT_EQ(1, segment_list->GetSegmentNum(2500000, period_duration_us));
  // 5 seconds in should be 3rd segment
  EXPECT_EQ(2, segment_list->GetSegmentNum(5000000, period_duration_us));

  uint64_t time_us = 0;
  EXPECT_EQ(time_us, segment_list->GetSegmentTimeUs(0));
  time_us += segment_duration * util::kMicrosPerSecond / timescale;
  EXPECT_EQ(time_us, segment_list->GetSegmentTimeUs(1));
  time_us += segment_duration * util::kMicrosPerSecond / timescale;
  EXPECT_EQ(time_us, segment_list->GetSegmentTimeUs(2));

  EXPECT_EQ(true, segment_list->IsExplicit());
}

}  // namespace mpd

}  // namespace ndash
