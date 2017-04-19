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

#include "upstream/data_spec.h"

#include <string>

#include "gtest/gtest.h"
#include "upstream/data_spec.h"
#include "upstream/uri.h"

namespace ndash {
namespace upstream {

TEST(DataSpecTest, UriOnlySpec) {
  const std::string kFileUriString("file:///tmp/uri_only_spec.txt");

  Uri file_uri(kFileUriString);
  DataSpec file_spec(file_uri);

  const std::string kExpectedDebugString(
      "DataSpec[file:///tmp/uri_only_spec.txt, (null), 0, 0, UNB, (null), 0]");

  EXPECT_EQ(kExpectedDebugString, file_spec.DebugString());
}

TEST(DataSpecTest, FlagsSpec) {
  const std::string kFileUriString("file:///tmp/flags_spec.txt");
  const int kUriFlags = 10;

  Uri file_uri(kFileUriString);
  DataSpec file_spec(file_uri, kUriFlags);

  const std::string kExpectedDebugString(
      "DataSpec[file:///tmp/flags_spec.txt, (null), 0, 0, UNB, (null), 10]");

  EXPECT_EQ(kExpectedDebugString, file_spec.DebugString());
}

TEST(DataSpecTest, PositionSpec) {
  const std::string kFileUriString("file:///tmp/position_spec.txt");
  const int64_t kPosition = 1234;
  const int64_t kLength = 4321;
  const std::string kKey = "abcd";

  Uri file_uri(kFileUriString);
  DataSpec file_spec(file_uri, kPosition, kLength, &kKey);

  const std::string kExpectedDebugString(
      "DataSpec[file:///tmp/position_spec.txt, (null), 1234, 1234, 4321, "
      "abcd, 0]");

  EXPECT_EQ(kExpectedDebugString, file_spec.DebugString());
}

TEST(DataSpecTest, PositionFlagsSpec) {
  const std::string kFileUriString("file:///tmp/position_flags_spec.txt");
  const int kFlags = 20;
  const int64_t kPosition = 2345;
  const int64_t kLength = 5432;
  const std::string kKey = "efgh";

  Uri file_uri(kFileUriString);
  DataSpec file_spec(file_uri, kPosition, kLength, &kKey, kFlags);

  const std::string kExpectedDebugString(
      "DataSpec[file:///tmp/position_flags_spec.txt, (null), 2345, 2345, 5432, "
      "efgh, 20]");

  EXPECT_EQ(kExpectedDebugString, file_spec.DebugString());
}

TEST(DataSpecTest, DiffPositionSpec) {
  const std::string kFileUriString("file:///tmp/diff_position_spec.txt");
  const int kFlags = 30;
  const int64_t kAbsolutePosition = 4567;
  const int64_t kPosition = 3456;
  const int64_t kLength = 6543;
  const std::string kKey = "ijkl";

  Uri file_uri(kFileUriString);
  DataSpec file_spec(file_uri, kAbsolutePosition, kPosition, kLength, &kKey,
                     kFlags);

  const std::string kExpectedDebugString(
      "DataSpec[file:///tmp/diff_position_spec.txt, (null), 4567, 3456, 6543, "
      "ijkl, 30]");

  EXPECT_EQ(kExpectedDebugString, file_spec.DebugString());
}

TEST(DataSpecTest, PostSpec) {
  const std::string kFileUriString("file:///tmp/post_spec.txt");
  const int kFlags = 40;
  const int64_t kAbsolutePosition = 5678;
  const int64_t kPosition = 4444;
  const int64_t kLength = 7654;
  const std::string kKey = "mnop";
  const std::string kPostBody = "POST";

  Uri file_uri(kFileUriString);
  DataSpec file_spec(file_uri, &kPostBody, kAbsolutePosition, kPosition,
                     kLength, &kKey, kFlags);

  const std::string kExpectedDebugString(
      "DataSpec[file:///tmp/post_spec.txt, POST, 5678, 4444, 7654, mnop, 40]");

  EXPECT_EQ(kExpectedDebugString, file_spec.DebugString());
}

}  // namespace upstream
}  // namespace ndash
