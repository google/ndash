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

#include "extractor/seek_map.h"
#include "extractor/seek_map_mock.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace ndash {
namespace extractor {

using ::testing::Eq;

TEST(SeekMapTest, CanInstantiateMock) {
  MockSeekMap seek_map;
}

TEST(SeekMapTest, TestUnseekable) {
  Unseekable unseekable;

  EXPECT_THAT(unseekable.IsSeekable(), Eq(false));
  EXPECT_THAT(unseekable.GetPosition(std::numeric_limits<int64_t>::min()),
              Eq(0));
  EXPECT_THAT(unseekable.GetPosition(-10), Eq(0));
  EXPECT_THAT(unseekable.GetPosition(0), Eq(0));
  EXPECT_THAT(unseekable.GetPosition(10), Eq(0));
  EXPECT_THAT(unseekable.GetPosition(std::numeric_limits<int64_t>::max()),
              Eq(0));
}

}  // namespace chunk
}  // namespace ndash
