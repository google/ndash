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

#include "util/util.h"

#include "gtest/gtest.h"

namespace ndash {

namespace util {

TEST(UtilTest, ScalePath1) {
  EXPECT_EQ(Util::ScaleLargeTimestamp(12345678, 1000000, 1000), 12345678000);
}

TEST(UtilTest, ScalePath2) {
  EXPECT_EQ(Util::ScaleLargeTimestamp(12345678, 1000, 1000000), 12345);
}

TEST(UtilTest, ScalePath3) {
  EXPECT_EQ(Util::ScaleLargeTimestamp(12345678, 1000, 37), 333666972);
}

TEST(UtilTest, CeilDivide) {
  EXPECT_EQ(Util::CeilDivide(10, 4), 3);
}

TEST(UtilTest, ParseXsDateTime) {
  std::string date_time = "2015-04-09T18:46:25";
  EXPECT_EQ(1428605185000LL, Util::ParseXsDateTime(date_time));
}

TEST(UtilTest, ParseXsDuration) {
  // Empty
  std::string duration = "";
  EXPECT_EQ(-1, Util::ParseXsDuration(duration));

  // No values
  duration = "P";
  EXPECT_EQ(-1, Util::ParseXsDuration(duration));

  // Single year
  duration = "P2Y";
  EXPECT_EQ(63113852000LL, Util::ParseXsDuration(duration));

  // Single month
  duration = "P3M";
  EXPECT_EQ(7889231250LL, Util::ParseXsDuration(duration));

  // Single day
  duration = "P4D";
  EXPECT_EQ(345600000LL, Util::ParseXsDuration(duration));

  // Single hour
  duration = "PT1H";
  EXPECT_EQ(3600000LL, Util::ParseXsDuration(duration));

  // Single minutes
  duration = "PT5M";
  EXPECT_EQ(300000LL, Util::ParseXsDuration(duration));

  // Single seconds
  duration = "PT37S";
  EXPECT_EQ(37000LL, Util::ParseXsDuration(duration));

  // Mixed no time component
  duration = "P1Y2M3D";
  EXPECT_EQ(37075613500LL, Util::ParseXsDuration(duration));

  // Mixed no date component
  duration = "PT12M3S";
  EXPECT_EQ(723000LL, Util::ParseXsDuration(duration));

  // Mixed all
  duration = "P1Y2M3DT1H2D3S";
  EXPECT_EQ(37079215500LL, Util::ParseXsDuration(duration));

  // Fraction for smallest component
  duration = "PT36.5S";
  EXPECT_EQ(36500LL, Util::ParseXsDuration(duration));

  // 365 days in seconds
  duration = "PT31536000S";
  EXPECT_EQ(31536000000LL, Util::ParseXsDuration(duration));

  // 32 years in seconds
  duration = "PT1009152000S";
  EXPECT_EQ(1009152000000LL, Util::ParseXsDuration(duration));
}

}  // namespace util

}  // namespace ndash
