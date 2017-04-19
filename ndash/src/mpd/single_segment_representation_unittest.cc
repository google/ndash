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

#include "mpd/single_segment_representation.h"

#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "mpd/mpd_unittest_helper.h"
#include "util/format.h"

namespace ndash {

namespace mpd {

TEST(SingleSegmentRepresentationTests, SingleSegmentRepresentationTest) {
  std::string content_id = "1234";
  int64_t revision_id = 0;
  util::Format format = CreateTestFormat();
  std::string cache_key = "my_custom_cache_key";

  std::string uri = "http://singlesegment";

  std::unique_ptr<SingleSegmentRepresentation> representation(
      SingleSegmentRepresentation::NewInstance(content_id, revision_id, format,
                                               uri, 0, 999, 1000, 1999,
                                               cache_key, 50000));

  EXPECT_EQ(cache_key, representation->GetCacheKey());
  EXPECT_EQ(format, representation->GetFormat());

  // Index is external so representation should provide an index.
  EXPECT_EQ(nullptr, representation->GetIndex());

  // Make sure range and url for the index are correct.
  const RangedUri* index_uri = representation->GetIndexUri();
  ASSERT_TRUE(index_uri != nullptr);
  EXPECT_EQ(1000, index_uri->GetLength());
  EXPECT_EQ(1000, index_uri->GetStart());
  EXPECT_EQ(uri, index_uri->GetUriString());
}

TEST(SingleSegmentRepresentationTests, DefaultCacheKey) {
  std::string content_id = "1234";
  int64_t revision_id = 0;
  util::Format format = CreateTestFormat();

  std::string uri = "http://singlesegment";
  std::unique_ptr<SingleSegmentRepresentation> representation(
      SingleSegmentRepresentation::NewInstance(
          content_id, revision_id, format, uri, 0, 999, 1000, 1999, "", 50000));

  EXPECT_EQ("1234.id1.0", representation->GetCacheKey());
}

}  // namespace mpd

}  // namespace ndash
