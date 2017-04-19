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
#include "mpd/segment_template.h"
#include "mpd/url_template.h"
#include "util/format.h"
#include "util/util.h"

namespace ndash {

namespace mpd {

TEST(RepresentationTests, RepSegmentTemplateWithInitializationTemplateTest) {
  // Simulate a 362 second period.
  int64_t period_duration = 362000;
  int64_t timescale = 1000;
  int64_t period_duration_us =
      period_duration * util::kMicrosPerSecond / timescale;
  int64_t segment_duration = 2500;
  // 362 seconds gives last segment with 2 seconds.
  int64_t last_partial_segment_duration = 2000;

  std::unique_ptr<SegmentTemplate> segment_template =
      CreateTestSegmentTemplate(timescale, segment_duration);

  std::unique_ptr<Representation> representation =
      CreateTestRepresentationWithSegmentTemplate(timescale, segment_duration,
                                                  segment_template.get());

  // Index is not external so representation should provide one.
  const DashSegmentIndexInterface* index = representation->GetIndex();
  ASSERT_TRUE(index != nullptr);
  // First segment should be 2.5 seconds
  EXPECT_EQ(segment_duration * util::kMicrosPerSecond / timescale,
            index->GetDurationUs(0, period_duration_us));
  // Same for a middle segment
  EXPECT_EQ(segment_duration * util::kMicrosPerSecond / timescale,
            index->GetDurationUs(72, period_duration_us));
  // Very last segment should be only 2 seconds.
  EXPECT_EQ(last_partial_segment_duration * util::kMicrosPerSecond / timescale,
            index->GetDurationUs(144, period_duration_us));

  // Start times should match our fixed durations.
  uint64_t time_us = 0;
  EXPECT_EQ(time_us, index->GetTimeUs(0));
  time_us += segment_duration * util::kMicrosPerSecond / timescale;
  EXPECT_EQ(time_us, index->GetTimeUs(1));
  time_us += segment_duration * util::kMicrosPerSecond / timescale;
  EXPECT_EQ(time_us, index->GetTimeUs(2));

  // Test upper/lower bounds.
  EXPECT_EQ(0, index->GetFirstSegmentNum());
  EXPECT_EQ(144, index->GetLastSegmentNum(period_duration_us));

  // 3x 2.5 seconds worth of data should be segment number 3
  EXPECT_EQ(3, index->GetSegmentNum(2500000 * 3, period_duration_us));
  EXPECT_NE(true, index->IsExplicit());
  std::unique_ptr<RangedUri> segment_url_0 = index->GetSegmentUrl(0);

  ASSERT_TRUE(segment_url_0.get() != nullptr);
  // Make sure template expands as expected.
  EXPECT_EQ("http://media/segment/id1/0/", segment_url_0->GetUriString());
  // There should be no range info.
  EXPECT_EQ(0, segment_url_0->GetStart());
  EXPECT_EQ(-1, segment_url_0->GetLength());

  std::unique_ptr<RangedUri> segment_url_72 = index->GetSegmentUrl(72);
  ASSERT_TRUE(segment_url_72.get() != nullptr);
  // Make sure template expands as expected.
  EXPECT_EQ("http://media/segment/id1/72/", segment_url_72->GetUriString());

  // There should be no index uri as it is computed from the template.
  EXPECT_EQ(nullptr, representation->GetIndexUri());

  // There should be no init url as we didn't give one.
  const RangedUri* init_uri = representation->GetInitializationUri();
  ASSERT_TRUE(init_uri != nullptr);
  // Make sure init template expands as expected.
  EXPECT_EQ("http://host/init/0/6000000", init_uri->GetUriString());
}

TEST(RepresentationTests, RepTemplateWithInitializationUrlTest) {
  std::string init_base_uri = "http://initialization";
  std::string init_reference_uri = "/initialize_me";
  std::string content_id = "1234";
  int64_t revision_id = 0;
  util::Format format = CreateTestFormat();
  std::unique_ptr<std::string> media_base_uri(new std::string("http://media"));

  std::unique_ptr<RangedUri> initialization(
      new RangedUri(&init_base_uri, init_reference_uri, 0, 10000));
  std::unique_ptr<UrlTemplate> media_template =
      mpd::UrlTemplate::Compile("segment/$RepresentationID$/$Number$/");

  // Use 2.5 second duration for our template.
  std::unique_ptr<SegmentBase> segment_template(new SegmentTemplate(
      std::move(media_base_uri), std::move(initialization), 1000, 0, 0, 2500,
      std::unique_ptr<std::vector<SegmentTimelineElement>>(nullptr),
      std::unique_ptr<UrlTemplate>(nullptr), std::move(media_template)));

  std::unique_ptr<Representation> representation =
      std::unique_ptr<Representation>(Representation::NewInstance(
          content_id, revision_id, format, std::move(segment_template)));
  // Representation should have taken ownership
  EXPECT_TRUE(representation->GetSegmentBase() != nullptr);

  // Make sure our initialization info matches what we passed in.
  const RangedUri* init_out = representation->GetInitializationUri();
  ASSERT_TRUE(init_out != nullptr);
  EXPECT_EQ("http://initialization/initialize_me", init_out->GetUriString());
  EXPECT_EQ(0, init_out->GetStart());
  EXPECT_EQ(10000, init_out->GetLength());
}

TEST(RepresentationTests, RepSegmentTemplateWithTimelineTest) {
  std::string content_id = "1234";
  int64_t revision_id = 0;
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
  std::unique_ptr<SegmentBase> segment_template(new SegmentTemplate(
      std::move(media_base_uri), std::unique_ptr<RangedUri>(nullptr), timescale,
      0, 0, 2500, std::move(timeline), std::move(init_template),
      std::move(media_template)));

  std::vector<std::unique_ptr<DescriptorType>> supplemental_properties;
  std::unique_ptr<DescriptorType> supplemental(
      new DescriptorType("scheme1", "value1", "id1"));
  supplemental_properties.push_back(std::move(supplemental));

  std::vector<std::unique_ptr<DescriptorType>> essential_properties;
  std::unique_ptr<DescriptorType> essential1(
      new DescriptorType("scheme1", "value1", "id1"));
  std::unique_ptr<DescriptorType> essential2(
      new DescriptorType("scheme2", "value2", "id2"));
  essential_properties.push_back(std::move(essential1));
  essential_properties.push_back(std::move(essential2));

  std::unique_ptr<Representation> representation =
      std::unique_ptr<Representation>(Representation::NewInstance(
          content_id, revision_id, format, std::move(segment_template), "",
          &supplemental_properties, &essential_properties));

  EXPECT_EQ(1, representation->GetSupplementalPropertyCount());
  EXPECT_EQ(2, representation->GetEssentialPropertyCount());

  // Test our index works with the timeline.
  const DashSegmentIndexInterface* index = representation->GetIndex();
  ASSERT_TRUE(index != nullptr);
  // Make sure durations match.
  int64_t d0_us = time0.GetDuration() * util::kMicrosPerSecond / timescale;
  int64_t d1_us = time1.GetDuration() * util::kMicrosPerSecond / timescale;
  int64_t d2_us = time2.GetDuration() * util::kMicrosPerSecond / timescale;
  EXPECT_EQ(d0_us, index->GetDurationUs(0, period_duration_us));
  EXPECT_EQ(d1_us, index->GetDurationUs(1, period_duration_us));
  EXPECT_EQ(d2_us, index->GetDurationUs(2, period_duration_us));

  // Start times should match our provided durations.
  uint64_t time_us = 0;
  EXPECT_EQ(time_us, index->GetTimeUs(0));
  time_us += time0.GetDuration() * util::kMicrosPerSecond / timescale;
  EXPECT_EQ(time_us, index->GetTimeUs(1));
  time_us += time1.GetDuration() * util::kMicrosPerSecond / timescale;
  EXPECT_EQ(time_us, index->GetTimeUs(2));

  // Test upper/lower bounds.
  EXPECT_EQ(0, index->GetFirstSegmentNum());
  EXPECT_EQ(2, index->GetLastSegmentNum(period_duration_us));

  std::unique_ptr<RangedUri> segment_url_0 = index->GetSegmentUrl(0);
  ASSERT_TRUE(segment_url_0.get() != nullptr);
  // Make sure template expands as expected.
  EXPECT_EQ("http://media/segment/id1/0/", segment_url_0->GetUriString());

  // There should be no index uri as it is computed from the template.
  EXPECT_EQ(nullptr, representation->GetIndexUri());

  // Make sure reverse index works.
  int64_t t0_us = time0.GetStartTime() * util::kMicrosPerSecond / timescale;
  int64_t t1_us = time1.GetStartTime() * util::kMicrosPerSecond / timescale;
  int64_t t2_us = time2.GetStartTime() * util::kMicrosPerSecond / timescale;

  EXPECT_EQ(0, index->GetSegmentNum(t0_us, period_duration_us));
  EXPECT_EQ(1, index->GetSegmentNum(t1_us, period_duration_us));
  EXPECT_EQ(2, index->GetSegmentNum(t2_us, period_duration_us));
  EXPECT_EQ(true, index->IsExplicit());
}

TEST(RepresentationTests, RepSegmentListWithoutTimelineTest) {
  std::string content_id = "1234";
  int64_t revision_id = 0;
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

  std::unique_ptr<SegmentBase> segment_list(new SegmentList(
      std::move(media_base_uri), std::move(initialization), timescale, 0, 0,
      segment_duration,
      std::unique_ptr<std::vector<SegmentTimelineElement>>(nullptr),
      std::move(media_list)));

  std::vector<std::unique_ptr<DescriptorType>> supplemental_properties;
  std::unique_ptr<DescriptorType> supplemental(
      new DescriptorType("scheme1", "value1", "id1"));
  supplemental_properties.push_back(std::move(supplemental));

  std::vector<std::unique_ptr<DescriptorType>> essential_properties;
  std::unique_ptr<DescriptorType> essential1(
      new DescriptorType("scheme1", "value1", "id1"));
  std::unique_ptr<DescriptorType> essential2(
      new DescriptorType("scheme2", "value2", "id2"));
  essential_properties.push_back(std::move(essential1));
  essential_properties.push_back(std::move(essential2));

  std::unique_ptr<Representation> representation =
      std::unique_ptr<Representation>(Representation::NewInstance(
          content_id, revision_id, format, std::move(segment_list), "",
          &supplemental_properties, &essential_properties));

  EXPECT_EQ(1, representation->GetSupplementalPropertyCount());
  EXPECT_EQ(2, representation->GetEssentialPropertyCount());

  // Index is not external so representation should provide one.
  const DashSegmentIndexInterface* index = representation->GetIndex();
  ASSERT_TRUE(index != nullptr);
  // First segment should be 2.5 seconds
  EXPECT_EQ(segment_duration * util::kMicrosPerSecond / timescale,
            index->GetDurationUs(0, period_duration_us));
  // Same for a middle segment
  EXPECT_EQ(segment_duration * util::kMicrosPerSecond / timescale,
            index->GetDurationUs(1, period_duration_us));
  // Very last segment should be only 2 seconds.
  EXPECT_EQ(last_partial_segment_duration * util::kMicrosPerSecond / timescale,
            index->GetDurationUs(2, period_duration_us));

  // Test upper/lower bounds.
  EXPECT_EQ(0, index->GetFirstSegmentNum());
  EXPECT_EQ(2, index->GetLastSegmentNum(period_duration_us));

  // 0 seconds in should be 1st segment
  EXPECT_EQ(0, index->GetSegmentNum(0, period_duration_us));
  // 2.5 seconds in should be 2nd segment
  EXPECT_EQ(1, index->GetSegmentNum(2500000, period_duration_us));
  // 5 seconds in should be 3rd segment
  EXPECT_EQ(2, index->GetSegmentNum(5000000, period_duration_us));

  // Start times should match our fixed durations.
  uint64_t time_us = 0;
  EXPECT_EQ(time_us, index->GetTimeUs(0));
  time_us += segment_duration * util::kMicrosPerSecond / timescale;
  EXPECT_EQ(time_us, index->GetTimeUs(1));
  time_us += segment_duration * util::kMicrosPerSecond / timescale;
  EXPECT_EQ(time_us, index->GetTimeUs(2));

  EXPECT_EQ(true, index->IsExplicit());

  std::unique_ptr<RangedUri> segment_url_0 = index->GetSegmentUrl(0);
  ASSERT_TRUE(segment_url_0.get() != nullptr);
  // We got the expected url?
  EXPECT_EQ("http://media/seg0", segment_url_0->GetUriString());
  // There should be no range info.
  EXPECT_EQ(0, segment_url_0->GetStart());
  EXPECT_EQ(-1, segment_url_0->GetLength());

  std::unique_ptr<RangedUri> segment_url_1 = index->GetSegmentUrl(1);
  ASSERT_TRUE(segment_url_1.get() != nullptr);
  // Make sure template expands as expected.
  EXPECT_EQ("http://media/seg1", segment_url_1->GetUriString());

  // There should be no index uri as it is computed from the template.
  EXPECT_EQ(nullptr, representation->GetIndexUri());

  // Make sure our initialization info matches what we passed in.
  const RangedUri* init_out = representation->GetInitializationUri();
  ASSERT_TRUE(init_out != nullptr);
  EXPECT_EQ("http://initialization/initialize_me", init_out->GetUriString());
  EXPECT_EQ(0, init_out->GetStart());
  EXPECT_EQ(10000, init_out->GetLength());
}

TEST(RepresentationTests, RepresentationWithBigPeriod) {
  // Large period (10 years) results in correct calculations.
  int64_t timescale = 1000;
  int64_t period_duration = 10 * 365 * 24 * 60 * 60 * timescale;
  int64_t period_duration_us =
      period_duration * util::kMicrosPerSecond / timescale;
  int64_t segment_duration = 2502;
  int64_t last_partial_segment_duration = 1170;

  std::unique_ptr<SegmentTemplate> segment_template =
      CreateTestSegmentTemplate(timescale, segment_duration);

  std::unique_ptr<Representation> representation =
      CreateTestRepresentationWithSegmentTemplate(timescale, segment_duration,
                                                  segment_template.get());

  // Index is not external so representation should provide one.
  const DashSegmentIndexInterface* index = representation->GetIndex();
  ASSERT_TRUE(index != nullptr);

  // Test upper/lower bounds.
  EXPECT_EQ(0, index->GetFirstSegmentNum());
  EXPECT_EQ(126043165, index->GetLastSegmentNum(period_duration_us));

  // First segment should be 2.5 seconds
  EXPECT_EQ(segment_duration * util::kMicrosPerSecond / timescale,
            index->GetDurationUs(0, period_duration_us));
  EXPECT_EQ(last_partial_segment_duration * util::kMicrosPerSecond / timescale,
            index->GetDurationUs(126043165, period_duration_us));

  EXPECT_EQ(3, index->GetSegmentNum(segment_duration * timescale * 3,
                                    period_duration_us));
}

}  // namespace mpd

}  // namespace ndash
