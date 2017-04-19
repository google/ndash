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

#include "util/mime_types.h"

#include <string>

#include "gtest/gtest.h"

namespace ndash {

namespace util {

TEST(MimeTypesTests, MimeTypeTest) {
  EXPECT_TRUE(MimeTypes::IsApplication("application/xpdf"));
  EXPECT_TRUE(MimeTypes::IsVideo("video/mpeg"));
  EXPECT_TRUE(MimeTypes::IsAudio("audio/mp4"));
  EXPECT_TRUE(MimeTypes::IsText("text/vtt"));
  EXPECT_TRUE(MimeTypes::IsText("application/x-rawcc"));
  EXPECT_FALSE(MimeTypes::IsApplication("other/xpdf"));
  EXPECT_FALSE(MimeTypes::IsVideo("other/mpeg"));
  EXPECT_FALSE(MimeTypes::IsAudio("other/mp4"));
  EXPECT_FALSE(MimeTypes::IsText("other/vtt"));

  std::string mime_type = "application/xpdf";
  base::StringPiece str;
  ASSERT_TRUE(MimeTypes::GetTopLevelType(mime_type, &str));
  EXPECT_EQ(str, "application");

  ASSERT_TRUE(MimeTypes::GetSubType(mime_type, &str));
  EXPECT_EQ(str, "xpdf");
}

}  // namespace util

}  // namespace ndash
