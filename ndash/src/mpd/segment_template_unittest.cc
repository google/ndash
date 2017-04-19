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
#include "mpd/segment_template.h"
#include "mpd/url_template.h"
#include "util/format.h"
#include "util/util.h"

namespace ndash {

namespace mpd {

TEST(SegmentTemplateTests, SegmentTemplateWithInitialization) {
  // Simulate a 362 second period.
  int64_t period_duration = 362000;
  int64_t timescale = 1000;
  int64_t period_duration_us =
      period_duration * util::kMicrosPerSecond / timescale;
  int64_t segment_duration = 2500;
  // 362 seconds gives last segment with 2 seconds.
  int64_t last_partial_segment_duration = 2000;

  // Use 2.5 second duration for our template.
  std::unique_ptr<SegmentTemplate> segment_template =
      CreateTestSegmentTemplate(timescale, segment_duration);

  // First segment should be 2.5 seconds
  EXPECT_EQ(segment_duration * util::kMicrosPerSecond / timescale,
            segment_template->GetSegmentDurationUs(0, period_duration_us));
  // Same for a middle segment
  EXPECT_EQ(segment_duration * util::kMicrosPerSecond / timescale,
            segment_template->GetSegmentDurationUs(72, period_duration_us));
  // Very last segment should be only 2 seconds.
  EXPECT_EQ(last_partial_segment_duration * util::kMicrosPerSecond / timescale,
            segment_template->GetSegmentDurationUs(144, period_duration_us));

  // Test upper/lower bounds.
  EXPECT_EQ(0, segment_template->GetFirstSegmentNum());
  EXPECT_EQ(144, segment_template->GetLastSegmentNum(period_duration_us));

  // 3x 2.5 seconds worth of data should be segment number 3
  EXPECT_EQ(3,
            segment_template->GetSegmentNum(2500000 * 3, period_duration_us));
  EXPECT_NE(true, segment_template->IsExplicit());

  // Start times should match our fixed durations.
  uint64_t time_us = 0;
  EXPECT_EQ(time_us, segment_template->GetSegmentTimeUs(0));
  time_us += segment_duration * util::kMicrosPerSecond / timescale;
  EXPECT_EQ(time_us, segment_template->GetSegmentTimeUs(1));
  time_us += segment_duration * util::kMicrosPerSecond / timescale;
  EXPECT_EQ(time_us, segment_template->GetSegmentTimeUs(2));
}

TEST(SegmentTemplateTests, SegmentTemplateWithoutInitialization) {
  std::string content_id = "1234";
  util::Format format = CreateTestFormat();
  std::unique_ptr<std::string> media_base_uri(new std::string("http://media"));

  int64_t segment_duration = 2500;
  int64_t timescale = 1000;

  std::unique_ptr<UrlTemplate> init_template =
      mpd::UrlTemplate::Compile("http://host/init/$Number$/$Bandwidth$");
  std::unique_ptr<UrlTemplate> media_template =
      mpd::UrlTemplate::Compile("segment/$RepresentationID$/$Number$/");

  // Use 2.5 second duration for our template.
  std::unique_ptr<SegmentTemplate> segment_template(new SegmentTemplate(
      std::move(media_base_uri), std::unique_ptr<RangedUri>(nullptr), 1000, 0,
      0, 2500, std::unique_ptr<std::vector<SegmentTimelineElement>>(nullptr),
      std::move(init_template), std::move(media_template)));

  // Start times should match our fixed durations.
  uint64_t time_us = 0;
  EXPECT_EQ(time_us, segment_template->GetSegmentTimeUs(0));
  time_us += segment_duration * util::kMicrosPerSecond / timescale;
  EXPECT_EQ(time_us, segment_template->GetSegmentTimeUs(1));
  time_us += segment_duration * util::kMicrosPerSecond / timescale;
  EXPECT_EQ(time_us, segment_template->GetSegmentTimeUs(2));
}

TEST(SegmentTemplateTests, SegmentTemplateWithTimeline) {
  std::string content_id = "1234";
  util::Format format = CreateTestFormat();
  std::unique_ptr<std::string> media_base_uri(new std::string("http://media"));

  std::unique_ptr<UrlTemplate> init_template =
      mpd::UrlTemplate::Compile("http://host/init/$Number$/$Bandwidth$");
  std::unique_ptr<UrlTemplate> media_template =
      mpd::UrlTemplate::Compile("segment/$RepresentationID$/$Number$/");

  // Use an irregular timeline.
  std::unique_ptr<std::vector<SegmentTimelineElement>> timeline(
      new std::vector<SegmentTimelineElement>());
  SegmentTimelineElement time0(0, 2500);
  SegmentTimelineElement time1(2500, 5000);
  SegmentTimelineElement time2(7500, 10000);
  timeline->push_back(time0);
  timeline->push_back(time1);
  timeline->push_back(time2);

  int64_t timescale = 1000;
  int64_t period_duration = 17500;
  int64_t period_duration_us =
      period_duration * util::kMicrosPerSecond / timescale;

  // Use 2.5 second duration for our template. Total 17.5 seconds in duration.
  std::unique_ptr<SegmentTemplate> segment_template(new SegmentTemplate(
      std::move(media_base_uri), std::unique_ptr<RangedUri>(nullptr), timescale,
      0, 0, 2500, std::move(timeline), std::move(init_template),
      std::move(media_template)));

  // Make sure durations match.
  int64_t d0_us = time0.GetDuration() * util::kMicrosPerSecond / timescale;
  int64_t d1_us = time1.GetDuration() * util::kMicrosPerSecond / timescale;
  int64_t d2_us = time2.GetDuration() * util::kMicrosPerSecond / timescale;
  EXPECT_EQ(d0_us,
            segment_template->GetSegmentDurationUs(0, period_duration_us));
  EXPECT_EQ(d1_us,
            segment_template->GetSegmentDurationUs(1, period_duration_us));
  EXPECT_EQ(d2_us,
            segment_template->GetSegmentDurationUs(2, period_duration_us));

  // Test upper/lower bounds.
  EXPECT_EQ(0, segment_template->GetFirstSegmentNum());
  EXPECT_EQ(2, segment_template->GetLastSegmentNum(period_duration_us));

  // Make sure reverse index works.
  int64_t t0_us = time0.GetStartTime() * util::kMicrosPerSecond / timescale;
  int64_t t1_us = time1.GetStartTime() * util::kMicrosPerSecond / timescale;
  int64_t t2_us = time2.GetStartTime() * util::kMicrosPerSecond / timescale;

  EXPECT_EQ(0, segment_template->GetSegmentNum(t0_us, period_duration_us));
  EXPECT_EQ(1, segment_template->GetSegmentNum(t1_us, period_duration_us));
  EXPECT_EQ(2, segment_template->GetSegmentNum(t2_us, period_duration_us));

  // Start times should match our provided durations.
  uint64_t time_us = 0;
  EXPECT_EQ(time_us, segment_template->GetSegmentTimeUs(0));
  time_us += time0.GetDuration() * util::kMicrosPerSecond / timescale;
  EXPECT_EQ(time_us, segment_template->GetSegmentTimeUs(1));
  time_us += time1.GetDuration() * util::kMicrosPerSecond / timescale;
  EXPECT_EQ(time_us, segment_template->GetSegmentTimeUs(2));

  EXPECT_EQ(true, segment_template->IsExplicit());
}

}  // namespace mpd

}  // namespace ndash
