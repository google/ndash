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

#include "mpd/media_presentation_description.h"

#include "gtest/gtest.h"
#include "mpd/mpd_unittest_helper.h"

namespace ndash {

namespace mpd {

TEST(MediaPresentationDescriptionTests, MediaPresentationDescriptionNoPeriods) {
  std::unique_ptr<DescriptorType> utc_timing =
      std::unique_ptr<DescriptorType>(new DescriptorType(
          "urn:mpeg:dash:utc:direct:2012", "2016-05-26T11:37:23.586Z"));

  std::vector<std::unique_ptr<Period>> periods;

  scoped_refptr<MediaPresentationDescription> mpd(
      new MediaPresentationDescription(15000, 0, 60000, false, 2500, 60000,
                                       std::move(utc_timing), "http://mpd",
                                       &periods));

  EXPECT_EQ(15000, mpd->GetAvailabilityStartTime());
  EXPECT_EQ(0, mpd->GetDuration());
  EXPECT_EQ(60000, mpd->GetMinBufferTime());
  EXPECT_TRUE(mpd->IsDynamic() == false);
  EXPECT_EQ(2500, mpd->GetMinUpdatePeriod());
  EXPECT_EQ(60000, mpd->GetTimeShiftBufferDepth());
  EXPECT_TRUE(mpd->GetUtcTiming() != nullptr);
  EXPECT_EQ("http://mpd", mpd->GetNextManifestUri());
  EXPECT_EQ(0, mpd->GetPeriodCount());
}

TEST(MediaPresentationDescriptionTests, MediaPresentationDescriptionOnePeriod) {
  std::unique_ptr<DescriptorType> utc_timing =
      std::unique_ptr<DescriptorType>(new DescriptorType(
          "urn:mpeg:dash:utc:direct:2012", "2016-05-26T11:37:23.586Z"));

  std::vector<std::unique_ptr<Period>> periods;
  periods.push_back(CreateTestPeriod(0, 1000, 2500));

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

  scoped_refptr<MediaPresentationDescription> mpd(
      new MediaPresentationDescription(15000, 362000, 60000, false, 2500, 60000,
                                       std::move(utc_timing), "http://mpd",
                                       &periods, &supplemental_properties,
                                       &essential_properties));

  EXPECT_EQ(15000, mpd->GetAvailabilityStartTime());
  EXPECT_EQ(362000, mpd->GetDuration());
  EXPECT_EQ(60000, mpd->GetMinBufferTime());
  EXPECT_TRUE(mpd->IsDynamic() == false);
  EXPECT_EQ(2500, mpd->GetMinUpdatePeriod());
  EXPECT_EQ(60000, mpd->GetTimeShiftBufferDepth());
  EXPECT_TRUE(mpd->GetUtcTiming() != nullptr);
  EXPECT_EQ("http://mpd", mpd->GetNextManifestUri());
  EXPECT_EQ(1, mpd->GetPeriodCount());
  EXPECT_EQ(362000, mpd->GetPeriodDuration(0));
  EXPECT_EQ(1, mpd->GetSupplementalPropertyCount());
  EXPECT_EQ(2, mpd->GetEssentialPropertyCount());
}

TEST(MediaPresentationDescriptionTests, MediaPresentationDescriptionPeriodDur) {
  std::unique_ptr<DescriptorType> utc_timing =
      std::unique_ptr<DescriptorType>(new DescriptorType(
          "urn:mpeg:dash:utc:direct:2012", "2016-05-26T11:37:23.586Z"));

  std::vector<std::unique_ptr<Period>> periods;
  periods.push_back(CreateTestPeriod(0, 1000, 2500));
  periods.push_back(CreateTestPeriod(160000, 1000, 2500));

  scoped_refptr<MediaPresentationDescription> mpd(
      new MediaPresentationDescription(15000, 362000, 60000, false, 2500, 60000,
                                       std::move(utc_timing), "http://mpd",
                                       &periods));

  EXPECT_EQ("http://mpd", mpd->GetNextManifestUri());
  EXPECT_EQ(2, mpd->GetPeriodCount());
  EXPECT_EQ(160000, mpd->GetPeriodDuration(0));
  EXPECT_EQ(202000, mpd->GetPeriodDuration(1));

  // Out of bounds
  EXPECT_EQ(-1, mpd->GetPeriodDuration(-1));
  EXPECT_EQ(-1, mpd->GetPeriodDuration(2));
}

TEST(MediaPresentationDescriptionTests,
     MediaPresentationDescriptionPeriodUnknownDur) {
  std::unique_ptr<DescriptorType> utc_timing =
      std::unique_ptr<DescriptorType>(new DescriptorType(
          "urn:mpeg:dash:utc:direct:2012", "2016-05-26T11:37:23.586Z"));

  std::vector<std::unique_ptr<Period>> periods;
  periods.push_back(CreateTestPeriod(0, 1000, 2500));
  periods.push_back(CreateTestPeriod(160000, 1000, 2500));

  scoped_refptr<MediaPresentationDescription> mpd(
      new MediaPresentationDescription(15000, -1, 60000, false, 2500, 60000,
                                       std::move(utc_timing), "http://mpd",
                                       &periods));

  EXPECT_EQ(2, mpd->GetPeriodCount());
  EXPECT_EQ(160000, mpd->GetPeriodDuration(0));
  EXPECT_EQ(-1, mpd->GetPeriodDuration(1));
}

// Tests the last period duration calculation does not assume the first period
// starts at 0.
TEST(MediaPresentationDescriptionTests,
     MediaPresentationDescriptionPeriodLastPeriodDuration) {
  std::unique_ptr<DescriptorType> utc_timing =
      std::unique_ptr<DescriptorType>(new DescriptorType(
          "urn:mpeg:dash:utc:direct:2012", "2016-05-26T11:37:23.586Z"));

  std::vector<std::unique_ptr<Period>> periods;
  periods.push_back(CreateTestPeriod(12543099000000, 1000, 2500));
  periods.push_back(CreateTestPeriod(12544310000000, 1000, 2500));

  scoped_refptr<MediaPresentationDescription> mpd(
      new MediaPresentationDescription(0, 3600000000, 60000, false, 2500, 60000,
                                       std::move(utc_timing), "http://mpd",
                                       &periods));

  EXPECT_EQ(2, mpd->GetPeriodCount());
  EXPECT_EQ(1211000000, mpd->GetPeriodDuration(0));
  EXPECT_EQ(2389000000, mpd->GetPeriodDuration(1));
}

}  // namespace mpd

}  // namespace ndash
