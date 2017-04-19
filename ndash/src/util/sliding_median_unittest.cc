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

#include "util/sliding_median.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace ndash {
namespace util {

using ::testing::Eq;

TEST(SlidingMedianTest, NoSamples) {
  SlidingMedian median(1);

  EXPECT_THAT(median.HasSample(), Eq(false));
  EXPECT_THAT(median.GetAverage(), Eq(0));  // As per API
}

TEST(SlidingMedianTest, ReplaceByOneSample) {
  constexpr SlidingMedian::SampleWeight kWeight = 1000;
  constexpr SlidingMedian::SampleValue values[] = {1, 5, 100, -500, -10, 0, 42};

  SlidingMedian median(kWeight);

  for (SlidingMedian::SampleValue value : values) {
    median.AddSample(kWeight, value);
    EXPECT_THAT(median.GetAverage(), Eq(value));
  }
}

TEST(SlidingMedianTest, InitialBuildUpAndWeights) {
  struct SampleData {
    SlidingMedian::SampleWeight input_weight;
    SlidingMedian::SampleValue input_value;
    SlidingMedian::SampleValue output_value;
  };

  constexpr SlidingMedian::SampleWeight kTotalWeight = 70;
  const struct SampleData sample_data[] = {
      {10, 5, 5},  // Weight 10
      // 5555555555XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
      //     ^     |
      {20, 7, 7},  // Weight 30
      // 555555555577777777777777777777XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
      //               ^               |
      {15, 3, 5},  // Weight 45
      // 333333333333333555555555577777777777777777777XXXXXXXXXXXXXXXXXXXXXXXXX
      //                       ^                      |
      {5, 6, 5},  // Even weight (50), round the target weight down
      // 33333333333333355555555556666677777777777777777777XXXXXXXXXXXXXXXXXXXX
      //                         ^                         |
      {1, 8, 6},  // Odd weight (51), use sample at weight 25
      // 333333333333333555555555566666777777777777777777778XXXXXXXXXXXXXXXXXXX
      //                          ^                         |
      {2, 4, 5},  // Weight 53, median nudged back down
      // 33333333333333344555555555566666777777777777777777778XXXXXXXXXXXXXXXXX
      //                           ^                          |
      {9, 1, 5},  // Weight 62
      // 11111111133333333333333344555555555566666777777777777777777778XXXXXXXX
      //                               ^                               |
      {15, 9, 7},  // Weight 77 (clamped to 70), expired some of the 5's
      // 1111111113333333333333334455566666777777777777777777778999999999999999
      //                                   ^                                   |
  };

  SlidingMedian median(kTotalWeight);

  for (const struct SampleData& sample : sample_data) {
    median.AddSample(sample.input_weight, sample.input_value);
    EXPECT_THAT(median.GetAverage(), Eq(sample.output_value));
  }
}

TEST(SlidingMedianTest, ReplacementEven) {
  constexpr SlidingMedian::SampleWeight kTotalWeight = 16;
  constexpr SlidingMedian::SampleValue kInitialValue = 7;
  SlidingMedian median(kTotalWeight);

  for (int i = 0; i < kTotalWeight; i++) {
    median.AddSample(1, i);
    EXPECT_THAT(median.GetAverage(), Eq((i / 2)));
  }

  EXPECT_THAT(median.GetAverage(), Eq(kInitialValue));

  for (int i = 0; i < kTotalWeight; i++) {
    // Add a sample, which will replace an identical sample just expiring
    median.AddSample(1, i);
    EXPECT_THAT(median.GetAverage(), Eq(kInitialValue));
  }

  // Add the same values in the reverse order, which will temporarily affect
  // the median (it will trend upwards) but result in the same value at the
  // end.
  constexpr SlidingMedian::SampleValue kExpectedValues[] = {
      0x8, 0x9, 0xA, 0xB, 0xB, 0xB, 0xB, 0xB,
      0xB, 0xB, 0xB, 0xB, 0xA, 0x9, 0x8, 0x7};

  // 0123456|7|89ABCDEF Initial
  // 1234567|8|9ABCDEFF i=0
  // 2345678|9|ABCDEEFF i=1
  // 3456789|A|BCDDEEFF i=2
  // 456789A|B|CCDDEEFF i=3
  // 56789AB|B|CCDDEEFF i=4
  // 6789AAB|B|CCDDEEFF i=5
  // 7899AAB|B|CCDDEEFF i=6
  // 8899AAB|B|CCDDEEFF i=7
  // 7899AAB|B|CCDDEEFF i=8
  // 6789AAB|B|CCDDEEFF i=9
  // 56789AB|B|CCDDEEFF i=A
  // 456789A|B|CCDDEEFF i=B
  // 3456789|A|BCDDEEFF i=C
  // 2345678|9|ABCDEEFF i=D
  // 1234567|8|9ABCDEFF i=E
  // 0123456|7|89ABCDEF i=F Done

  for (int i = 0; i < kTotalWeight; i++) {
    median.AddSample(1, kTotalWeight - i - 1);
    EXPECT_THAT(median.GetAverage(), Eq(kExpectedValues[i]));
  }

  EXPECT_THAT(median.GetAverage(), Eq(kInitialValue));
}

}  // namespace util
}  // namespace ndash
