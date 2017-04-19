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

#include "util/format.h"

#include <string>

#include "gtest/gtest.h"

namespace ndash {

namespace util {

TEST(FormatTest, ConstructorArgs) {
  Format f("id1", "video/mpeg4", 320, 480, 29.98, 1, 2, 48000, 6000000, "en_us",
           "codec");

  EXPECT_EQ("id1", f.GetId());
  EXPECT_EQ("video/mpeg4", f.GetMimeType());
  EXPECT_EQ(320, f.GetWidth());
  EXPECT_EQ(480, f.GetHeight());
  EXPECT_EQ(29.98, f.GetFrameRate());
  EXPECT_EQ(1, f.GetMaxPlayoutRate());
  EXPECT_EQ(2, f.GetAudioChannels());
  EXPECT_EQ(48000, f.GetAudioSamplingRate());
  EXPECT_EQ(6000000, f.GetBitrate());
  EXPECT_EQ("en_us", f.GetLanguage());
  EXPECT_EQ("codec", f.GetCodecs());
}

TEST(FormatTest, FormatEquality) {
  Format f1("id1", "video/mp4", 320, 480, 29.98, 1, 2, 48000, 5000000);
  Format f2("id2", "video/mp4", 320, 480, 29.98, 1, 2, 48000, 6000000);
  Format f3("id1", "video/mpeg4", 320, 480, 29.98, 1, 2, 48000, 6000000,
            "en_us", "codec");

  EXPECT_FALSE(f1 == f2);
  EXPECT_TRUE(f1 == f3);
}

}  // namespace util

}  // namespace ndash
