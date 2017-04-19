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

#include "playback_rate.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace ndash {

using ::testing::Eq;

TEST(PlaybackRateTest, ConstructorsAndAccessors) {
  PlaybackRate playback_rate_normal;
  PlaybackRate playback_rate_fast(2.0f);
  PlaybackRate playback_rate_back(-1.0f);

  EXPECT_THAT(playback_rate_normal.rate(), Eq(1.0f));
  EXPECT_THAT(playback_rate_fast.rate(), Eq(2.0f));
  EXPECT_THAT(playback_rate_back.rate(), Eq(-1.0f));

  playback_rate_back.set_rate(-3.0f);
  EXPECT_THAT(playback_rate_back.rate(), Eq(-3.0f));
}

TEST(PlaybackRateTest, IsX) {
  PlaybackRate playback_rate_normal(1.0f);
  PlaybackRate playback_rate_fast(2.0f);
  PlaybackRate playback_rate_back(-3.0f);

  // The behaviour when rate is set to 0 is arbitrary -- it's not obvious what
  // the correct answer is in many cases. To avoid accidentally introducing
  // subtle bugs based on changing the meaning of 0, let's bake them into the
  // unit tests for now. Intentional changes can update the unit tests as
  // needed.
  PlaybackRate playback_rate_pause(0.0f);

  EXPECT_THAT(playback_rate_normal.IsForward(), Eq(true));
  EXPECT_THAT(playback_rate_fast.IsForward(), Eq(true));
  EXPECT_THAT(playback_rate_back.IsForward(), Eq(false));
  EXPECT_THAT(playback_rate_pause.IsForward(), Eq(true));

  EXPECT_THAT(playback_rate_normal.IsNormal(), Eq(true));
  EXPECT_THAT(playback_rate_fast.IsNormal(), Eq(false));
  EXPECT_THAT(playback_rate_back.IsNormal(), Eq(false));
  EXPECT_THAT(playback_rate_pause.IsNormal(), Eq(false));

  EXPECT_THAT(playback_rate_normal.IsTrick(), Eq(false));
  EXPECT_THAT(playback_rate_fast.IsTrick(), Eq(true));
  EXPECT_THAT(playback_rate_back.IsTrick(), Eq(true));
  EXPECT_THAT(playback_rate_pause.IsTrick(), Eq(true));
}

TEST(PlaybackRateTest, GetMagnitude) {
  PlaybackRate playback_rate(1.0f);
  EXPECT_THAT(playback_rate.GetMagnitude(), Eq(1.0f));

  playback_rate.set_rate(2.0f);
  EXPECT_THAT(playback_rate.GetMagnitude(), Eq(2.0f));

  playback_rate.set_rate(5.0f);
  EXPECT_THAT(playback_rate.GetMagnitude(), Eq(5.0f));

  playback_rate.set_rate(-5.0f);
  EXPECT_THAT(playback_rate.GetMagnitude(), Eq(5.0f));

  playback_rate.set_rate(-3.0f);
  EXPECT_THAT(playback_rate.GetMagnitude(), Eq(3.0f));

  playback_rate.set_rate(0.0f);
  EXPECT_THAT(playback_rate.GetMagnitude(), Eq(0.0f));
}

}  // namespace ndash
