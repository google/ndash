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

#include "mpd/period.h"

#include <string>

#include "gtest/gtest.h"
#include "mpd/mpd_unittest_helper.h"

namespace ndash {

namespace mpd {

TEST(PeriodTests, PeriodTest) {
  std::unique_ptr<Period> period = CreateTestPeriod(0, 1000, 2500);

  EXPECT_EQ(0, period->GetAdaptationSetIndex(AdaptationType::VIDEO));
  EXPECT_EQ(1, period->GetAdaptationSetIndex(AdaptationType::AUDIO));
  EXPECT_EQ(2, period->GetAdaptationSetIndex(AdaptationType::TEXT));
  EXPECT_EQ("id", period->GetId());
  EXPECT_EQ(0, period->GetStartMs());
  EXPECT_EQ(3, period->GetAdaptationSets().size());
  EXPECT_EQ(1, period->GetSupplementalPropertyCount());
}

TEST(PeriodTests, PeriodTakesOwnershipOfSegmentBaseIfProvided) {
  std::unique_ptr<SegmentBase> ssb = CreateTestSingleSegmentBase();
  std::vector<std::unique_ptr<AdaptationSet>> adaptation_sets;
  std::unique_ptr<Period> period(
      new Period("id", 0, &adaptation_sets, std::move(ssb)));
  EXPECT_TRUE(period->GetSegmentBase() != nullptr);
}

}  // namespace mpd

}  // namespace ndash
