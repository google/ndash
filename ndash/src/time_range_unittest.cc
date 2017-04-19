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


#include "time_range.h"
#include "time_range_mock.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "base/time/tick_clock.h"
#include "base/time/time.h"

namespace ndash {

class MockTickClock : public base::TickClock {
 public:
  MockTickClock() {}
  ~MockTickClock() override {}

  MOCK_METHOD0(NowTicks, base::TimeTicks());
};

using ::testing::Eq;
using ::testing::Mock;
using ::testing::Ne;
using ::testing::Return;

TEST(TimeRangeTest, CanInstantiateMock) {
  MockTimeRange mock_time_range;
}

TEST(StaticTimeRangeTest, TestIsStatic) {
  const base::TimeDelta start = base::TimeDelta::FromHours(1);
  const base::TimeDelta end = base::TimeDelta::FromHours(3);
  StaticTimeRange range(start, end);

  EXPECT_THAT(range.IsStatic(), Eq(true));
}

TEST(StaticTimeRangeTest, Test2ArgConstructor) {
  const base::TimeDelta start = base::TimeDelta::FromHours(1);
  const base::TimeDelta end = base::TimeDelta::FromHours(3);
  const base::TimeDelta difference = base::TimeDelta::FromHours(2);

  StaticTimeRange range(start, end);
  TimeRangeInterface::TimeDeltaPair bounds = range.GetCurrentBounds();

  EXPECT_THAT(bounds.first, Eq(start));
  EXPECT_THAT(bounds.second, Eq(end));
  EXPECT_THAT(bounds.second - bounds.first, Eq(difference));
}

TEST(StaticTimeRangeTest, Test1ArgConstructor) {
  const base::TimeDelta start = base::TimeDelta::FromHours(2);
  const base::TimeDelta end = base::TimeDelta::FromHours(5);
  const base::TimeDelta difference = base::TimeDelta::FromHours(3);

  StaticTimeRange range(std::make_pair(start, end));
  TimeRangeInterface::TimeDeltaPair bounds = range.GetCurrentBounds();

  EXPECT_THAT(bounds.first, Eq(start));
  EXPECT_THAT(bounds.second, Eq(end));
  EXPECT_THAT(bounds.second - bounds.first, Eq(difference));
}

TEST(StaticTimeRangeTest, Test0ArgConstructor) {
  StaticTimeRange range;
  TimeRangeInterface::TimeDeltaPair bounds = range.GetCurrentBounds();

  EXPECT_THAT(bounds.first.is_zero(), Eq(true));
  EXPECT_THAT(bounds.second.is_zero(), Eq(true));
}

TEST(StaticTimeRangeTest, TestCopyConstructor) {
  const base::TimeDelta start = base::TimeDelta::FromHours(5);
  const base::TimeDelta end = base::TimeDelta::FromHours(6);
  const base::TimeDelta difference = base::TimeDelta::FromHours(1);

  StaticTimeRange range1(start, end);
  TimeRangeInterface::TimeDeltaPair bounds1 = range1.GetCurrentBounds();

  StaticTimeRange range2(range1);
  TimeRangeInterface::TimeDeltaPair bounds2 = range2.GetCurrentBounds();

  EXPECT_THAT(bounds2.first, Eq(start));
  EXPECT_THAT(bounds2.second, Eq(end));
  EXPECT_THAT(bounds2.second - bounds2.first, Eq(difference));
  EXPECT_THAT(bounds2, Eq(bounds1));
}

TEST(StaticTimeRangeTest, TestEqual) {
  const base::TimeDelta start1 = base::TimeDelta::FromHours(5);
  const base::TimeDelta start2 = base::TimeDelta::FromHours(6);
  const base::TimeDelta end1 = base::TimeDelta::FromHours(8);
  const base::TimeDelta end2 = base::TimeDelta::FromHours(7);

  StaticTimeRange range1_1(start1, end1);
  StaticTimeRange range1_2(start1, end2);
  StaticTimeRange range2_1(start2, end1);
  StaticTimeRange range2_2(start2, end2);
  StaticTimeRange range1_1_constructed(start1, end1);
  StaticTimeRange range1_1_copied(range1_1);

  // Half of these checks are redundant but verify that if a != b then b != a.
  // range1_1_constructed and range1_1_copied are checked less exhastively
  // because presumably there's a lot of overlap with range1_1
  EXPECT_THAT(range1_1, Eq(range1_1));
  EXPECT_THAT(range1_1, Ne(range1_2));
  EXPECT_THAT(range1_1, Ne(range2_1));
  EXPECT_THAT(range1_1, Ne(range2_2));
  EXPECT_THAT(range1_1, Eq(range1_1_constructed));
  EXPECT_THAT(range1_1_constructed, Eq(range1_1));
  EXPECT_THAT(range1_1, Eq(range1_1_copied));
  EXPECT_THAT(range1_1_copied, Eq(range1_1));

  EXPECT_THAT(range1_2, Ne(range1_1));
  EXPECT_THAT(range1_2, Eq(range1_2));
  EXPECT_THAT(range1_2, Ne(range2_1));
  EXPECT_THAT(range1_2, Ne(range2_2));
  EXPECT_THAT(range1_2, Ne(range1_1_constructed));
  EXPECT_THAT(range1_2, Ne(range1_1_copied));

  EXPECT_THAT(range2_1, Ne(range1_1));
  EXPECT_THAT(range2_1, Ne(range1_2));
  EXPECT_THAT(range2_1, Eq(range2_1));
  EXPECT_THAT(range2_1, Ne(range2_2));

  EXPECT_THAT(range2_2, Ne(range1_1));
  EXPECT_THAT(range2_2, Ne(range1_2));
  EXPECT_THAT(range2_2, Ne(range2_1));
  EXPECT_THAT(range2_2, Eq(range2_2));
}

TEST(DynamicTimeRangeTest, TestIsStatic) {
  const base::TimeDelta start = base::TimeDelta::FromHours(1);
  const base::TimeDelta end = base::TimeDelta::FromHours(10);
  const base::TimeDelta buffer = base::TimeDelta::FromHours(1);
  const base::TimeTicks start_time = base::TimeTicks::Now();
  MockTickClock clock;

  DynamicTimeRange range(start, end, start_time, buffer, &clock);
  EXPECT_THAT(range.IsStatic(), Eq(false));
}

TEST(DynamicTimeRangeTest, TestGetCurrentBounds) {
  const base::TimeDelta start = base::TimeDelta::FromHours(1);
  const base::TimeDelta end = base::TimeDelta::FromHours(10);
  const base::TimeDelta no_buffer;
  const base::TimeDelta buffer = base::TimeDelta::FromHours(2);
  const base::TimeTicks start_time = base::TimeTicks::Now();
  MockTickClock clock;

  DynamicTimeRange::TimeDeltaPair bounds;

  DynamicTimeRange range_nobuffer(start, end, start_time, no_buffer, &clock);
  DynamicTimeRange range_buffer(start, end, start_time, buffer, &clock);

  // Start of time range
  EXPECT_CALL(clock, NowTicks()).Times(2).WillRepeatedly(Return(start_time));

  bounds = range_buffer.GetCurrentBounds();
  EXPECT_THAT(bounds.first, Eq(start));
  EXPECT_THAT(bounds.second, Eq(base::TimeDelta()));

  bounds = range_nobuffer.GetCurrentBounds();
  EXPECT_THAT(bounds.first, Eq(start));
  EXPECT_THAT(bounds.second, Eq(base::TimeDelta()));

  Mock::VerifyAndClearExpectations(&clock);

  // Soon after start of time range
  const base::TimeDelta offset1 = base::TimeDelta::FromHours(1);
  EXPECT_CALL(clock, NowTicks())
      .Times(2)
      .WillRepeatedly(Return(start_time + offset1));

  bounds = range_buffer.GetCurrentBounds();
  EXPECT_THAT(bounds.first, Eq(start));
  EXPECT_THAT(bounds.second, Eq(offset1));

  bounds = range_nobuffer.GetCurrentBounds();
  EXPECT_THAT(bounds.first, Eq(start));
  EXPECT_THAT(bounds.second, Eq(offset1));

  Mock::VerifyAndClearExpectations(&clock);

  // Part way through time range
  const base::TimeDelta offset2 = base::TimeDelta::FromHours(6);
  EXPECT_CALL(clock, NowTicks())
      .Times(2)
      .WillRepeatedly(Return(start_time + offset2));

  bounds = range_buffer.GetCurrentBounds();
  EXPECT_THAT(bounds.first, Eq(offset2 - buffer));
  EXPECT_THAT(bounds.second, Eq(offset2));

  bounds = range_nobuffer.GetCurrentBounds();
  EXPECT_THAT(bounds.first, Eq(start));
  EXPECT_THAT(bounds.second, Eq(offset2));

  Mock::VerifyAndClearExpectations(&clock);

  // Past end of range
  const base::TimeDelta offset3 = base::TimeDelta::FromHours(11);
  EXPECT_CALL(clock, NowTicks())
      .Times(2)
      .WillRepeatedly(Return(start_time + offset3));

  bounds = range_buffer.GetCurrentBounds();
  EXPECT_THAT(bounds.first, Eq(end - buffer));
  EXPECT_THAT(bounds.second, Eq(end));

  bounds = range_nobuffer.GetCurrentBounds();
  EXPECT_THAT(bounds.first, Eq(start));
  EXPECT_THAT(bounds.second, Eq(end));

  Mock::VerifyAndClearExpectations(&clock);
}

TEST(DynamicTimeRangeTest, TestCopyConstructor) {
  const base::TimeDelta start = base::TimeDelta::FromHours(1);
  const base::TimeDelta end = base::TimeDelta::FromHours(10);
  const base::TimeDelta no_buffer;
  const base::TimeDelta buffer = base::TimeDelta::FromHours(2);
  const base::TimeDelta offset = base::TimeDelta::FromHours(6);
  const base::TimeTicks start_time = base::TimeTicks::Now();
  MockTickClock clock;

  DynamicTimeRange range_buffer(start, end, start_time, buffer, &clock);
  DynamicTimeRange range_buffer_copy(range_buffer);

  DynamicTimeRange range_nobuffer(start, end, start_time, no_buffer, &clock);
  DynamicTimeRange range_nobuffer_copy(range_nobuffer);

  EXPECT_CALL(clock, NowTicks()).WillRepeatedly(Return(start_time + offset));

  DynamicTimeRange::TimeDeltaPair bounds_original =
      range_buffer.GetCurrentBounds();
  DynamicTimeRange::TimeDeltaPair bounds_copy =
      range_buffer_copy.GetCurrentBounds();
  EXPECT_THAT(bounds_copy, Eq(bounds_original));

  bounds_original = range_nobuffer.GetCurrentBounds();
  bounds_copy = range_nobuffer_copy.GetCurrentBounds();
  EXPECT_THAT(bounds_copy, Eq(bounds_original));
}

TEST(DynamicTimeRangeTest, TestEqual) {
  const base::TimeDelta start1 = base::TimeDelta::FromHours(5);
  const base::TimeDelta start2 = base::TimeDelta::FromHours(6);
  const base::TimeDelta end1 = base::TimeDelta::FromHours(8);
  const base::TimeDelta end2 = base::TimeDelta::FromHours(7);
  const base::TimeDelta buffer = base::TimeDelta::FromHours(2);
  const base::TimeDelta no_buffer;
  const base::TimeTicks start_time1 = base::TimeTicks::Now();
  const base::TimeTicks start_time2 =
      start_time1 + base::TimeDelta::FromSeconds(1);
  MockTickClock clock;

  EXPECT_CALL(clock, NowTicks()).Times(0);

  DynamicTimeRange range1_1b(start1, end1, start_time1, buffer, &clock);
  DynamicTimeRange range1_2b(start1, end2, start_time1, buffer, &clock);
  DynamicTimeRange range2_1b(start2, end1, start_time1, buffer, &clock);
  DynamicTimeRange range2_2b(start2, end2, start_time1, buffer, &clock);
  DynamicTimeRange range1_1b_constructed(start1, end1, start_time1, buffer,
                                         &clock);
  DynamicTimeRange range1_1b_copied(range1_1b);
  DynamicTimeRange range1_1u(start1, end1, start_time1, no_buffer, &clock);
  DynamicTimeRange range2_2u(start2, end2, start_time1, no_buffer, &clock);
  DynamicTimeRange range1_1t2(start1, end1, start_time2, buffer, &clock);

  // Half of these checks are redundant but verify that if a != b then b != a.
  // The cross product of ranges here is large, so not all cases are covered
  EXPECT_THAT(range1_1b, Eq(range1_1b));
  EXPECT_THAT(range1_1b, Ne(range1_2b));
  EXPECT_THAT(range1_1b, Ne(range2_1b));
  EXPECT_THAT(range1_1b, Ne(range2_2b));
  EXPECT_THAT(range1_1b, Ne(range1_1u));
  EXPECT_THAT(range1_1b, Ne(range2_2u));
  EXPECT_THAT(range1_1b, Eq(range1_1b_constructed));
  EXPECT_THAT(range1_1b_constructed, Eq(range1_1b));
  EXPECT_THAT(range1_1b, Eq(range1_1b_copied));
  EXPECT_THAT(range1_1b_copied, Eq(range1_1b));
  EXPECT_THAT(range1_1b, Ne(range1_1t2));

  EXPECT_THAT(range1_2b, Ne(range1_1b));
  EXPECT_THAT(range1_2b, Eq(range1_2b));
  EXPECT_THAT(range1_2b, Ne(range2_1b));
  EXPECT_THAT(range1_2b, Ne(range2_2b));
  EXPECT_THAT(range1_2b, Ne(range1_1u));
  EXPECT_THAT(range1_2b, Ne(range2_2u));
  EXPECT_THAT(range1_2b, Ne(range1_1b_constructed));
  EXPECT_THAT(range1_2b, Ne(range1_1b_copied));
  EXPECT_THAT(range1_2b, Ne(range1_1t2));

  EXPECT_THAT(range2_1b, Ne(range1_1b));
  EXPECT_THAT(range2_1b, Ne(range1_2b));
  EXPECT_THAT(range2_1b, Eq(range2_1b));
  EXPECT_THAT(range2_1b, Ne(range2_2b));
  EXPECT_THAT(range2_1b, Ne(range1_1u));
  EXPECT_THAT(range2_1b, Ne(range2_2u));
  EXPECT_THAT(range2_1b, Ne(range1_1t2));

  EXPECT_THAT(range2_2b, Ne(range1_1b));
  EXPECT_THAT(range2_2b, Ne(range1_2b));
  EXPECT_THAT(range2_2b, Ne(range2_1b));
  EXPECT_THAT(range2_2b, Eq(range2_2b));
  EXPECT_THAT(range2_2b, Ne(range1_1u));
  EXPECT_THAT(range2_2b, Ne(range2_2u));
  EXPECT_THAT(range2_2b, Ne(range1_1t2));

  EXPECT_THAT(range1_1u, Ne(range1_1b));
  EXPECT_THAT(range1_1u, Ne(range1_2b));
  EXPECT_THAT(range1_1u, Ne(range2_1b));
  EXPECT_THAT(range1_1u, Ne(range2_2b));
  EXPECT_THAT(range1_1u, Eq(range1_1u));
  EXPECT_THAT(range1_1u, Ne(range2_2u));
  EXPECT_THAT(range1_1u, Ne(range1_1t2));

  EXPECT_THAT(range2_2u, Ne(range1_1b));
  EXPECT_THAT(range2_2u, Ne(range1_2b));
  EXPECT_THAT(range2_2u, Ne(range2_1b));
  EXPECT_THAT(range2_2u, Ne(range2_2b));
  EXPECT_THAT(range2_2u, Ne(range1_1u));
  EXPECT_THAT(range2_2u, Eq(range2_2u));
  EXPECT_THAT(range2_2u, Ne(range1_1t2));

  EXPECT_THAT(range1_1t2, Ne(range1_1b));
  EXPECT_THAT(range1_1t2, Ne(range1_2b));
  EXPECT_THAT(range1_1t2, Ne(range2_1b));
  EXPECT_THAT(range1_1t2, Ne(range2_2b));
  EXPECT_THAT(range1_1t2, Ne(range1_1u));
  EXPECT_THAT(range1_1t2, Ne(range2_2u));
  EXPECT_THAT(range1_1t2, Eq(range1_1t2));
}

}  // namespace ndash
