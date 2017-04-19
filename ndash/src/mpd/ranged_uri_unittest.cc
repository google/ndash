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

#include "mpd/ranged_uri.h"

#include <string>

#include "gtest/gtest.h"

namespace ndash {
namespace mpd {

TEST(RangedUriTests, Constructor) {
  std::string base_uri = "http://somewhere/";
  RangedUri ranged_uri(&base_uri, "/segment1", 123, 10);
  EXPECT_EQ(123, ranged_uri.GetStart());
  EXPECT_EQ(10, ranged_uri.GetLength());
}

TEST(RangedUriTests, GetUriStringTest) {
  std::string base_uri = "http://somewhere/";
  RangedUri ranged_uri(&base_uri, "/segment1", 123, 10);
  EXPECT_EQ("http://somewhere/segment1", ranged_uri.GetUriString());
}

TEST(RangedUriTests, Equality) {
  std::string base_uri = "http://somewhere/";
  RangedUri ranged_uri1(&base_uri, "/segment1", 123, 10);
  RangedUri ranged_uri2(&base_uri, "/segment1", 123, 10);
  EXPECT_TRUE(ranged_uri1 == ranged_uri2);
}

TEST(RangedUriTests, Inequality) {
  std::string base_uri = "http://somewhere/";
  std::string base_uri2 = "http://somewhere2/";
  RangedUri ranged_uri1(&base_uri, "/segment1", 123, 10);
  RangedUri ranged_uri2(&base_uri, "/segment2", 123, 10);
  RangedUri ranged_uri3(&base_uri2, "/segment1", 123, 10);
  EXPECT_FALSE(ranged_uri1 == ranged_uri2);
  EXPECT_FALSE(ranged_uri1 == ranged_uri3);
}

TEST(RangedUriTests, AttemptMerge) {
  std::string base_uri = "http://somewhere/";
  RangedUri ranged_uri1(&base_uri, "/segment1", 123, 10);
  RangedUri ranged_uri2(&base_uri, "/segment1", 133, 10);
  RangedUri ranged_uri3(&base_uri, "/segment1", 5, 10);
  RangedUri ranged_uri4(&base_uri, "/segment2", 133, 10);

  // Null in should produce null out
  EXPECT_EQ(nullptr, ranged_uri1.AttemptMerge(nullptr));

  // Adjacent
  std::unique_ptr<RangedUri> merged1 = ranged_uri1.AttemptMerge(&ranged_uri2);
  EXPECT_NE(nullptr, merged1);
  EXPECT_EQ(123, merged1->GetStart());
  EXPECT_EQ(20, merged1->GetLength());

  // Adjacent opposite dir
  std::unique_ptr<RangedUri> merged2 = ranged_uri2.AttemptMerge(&ranged_uri1);
  EXPECT_NE(nullptr, merged1);
  EXPECT_EQ(123, merged1->GetStart());
  EXPECT_EQ(20, merged1->GetLength());

  // Non adjacent should return nullptr
  std::unique_ptr<RangedUri> merged3 = ranged_uri3.AttemptMerge(&ranged_uri1);
  EXPECT_EQ(nullptr, merged3);

  // Can't merge when segment is not the same
  std::unique_ptr<RangedUri> merged4 = ranged_uri1.AttemptMerge(&ranged_uri4);
  EXPECT_EQ(nullptr, merged4);
}

}  // namespace mpd
}  // namespace ndash
