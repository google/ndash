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

#include "drm/scheme_init_data.h"

#include "gtest/gtest.h"
#include "util/uuid.h"

namespace ndash {
namespace drm {

namespace {
std::unique_ptr<char[]> CreateData(int32_t length) {
  std::unique_ptr<char[]> data(new char[length]());
  for (int i = 0; i < length; i++) {
    data[i] = i;
  }
  return data;
}
}  // namespace

TEST(SchemeInitDataTests, SchemeInitDataTest) {
  std::string mime_type = "widevine";
  util::Uuid uuid("09514A5C-F8EB-4B5F-B0C3-97F52B47AE8A");

  int32_t length = 10;
  std::unique_ptr<char[]> data = CreateData(length);

  SchemeInitData scheme_init(mime_type, std::move(data), length);
  EXPECT_TRUE(scheme_init == SchemeInitData(scheme_init));

  // Constructor
  EXPECT_NE(nullptr, scheme_init.GetData());
  EXPECT_EQ(length, scheme_init.GetLen());
  EXPECT_EQ(mime_type, scheme_init.GetMimeType());

  // Equality
  std::unique_ptr<char[]> data2 = CreateData(length);
  SchemeInitData scheme_init2(mime_type, std::move(data2), length);
  EXPECT_TRUE(scheme_init == scheme_init2);
  EXPECT_TRUE(scheme_init2 == SchemeInitData(scheme_init2));

  // Inequality by data content
  std::unique_ptr<char[]> data3 = CreateData(length);
  data3[0] = -1;
  SchemeInitData scheme_init3(mime_type, std::move(data3), length);
  EXPECT_FALSE(scheme_init == scheme_init3);
  EXPECT_TRUE(scheme_init3 == SchemeInitData(scheme_init3));

  // Inequality by data length
  std::unique_ptr<char[]> data4 = CreateData(length - 1);
  SchemeInitData scheme_init4(mime_type, std::move(data4), length - 1);
  EXPECT_FALSE(scheme_init == scheme_init4);
  EXPECT_TRUE(scheme_init4 == SchemeInitData(scheme_init4));
}

}  // namespace drm
}  // namespace ndash
