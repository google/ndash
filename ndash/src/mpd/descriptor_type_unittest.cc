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

#include "descriptor_type.h"

#include <string>

#include "gtest/gtest.h"

namespace ndash {
namespace mpd {

TEST(DescriptorTypeTest, ConstructorArgs) {
  DescriptorType descriptor("http://some.uri", "1", "id");
  EXPECT_EQ("http://some.uri", descriptor.scheme_id_uri());
  EXPECT_EQ("1", descriptor.value());
  EXPECT_EQ("id", descriptor.id());
}

TEST(DescriptorTypeTest, Copy) {
  DescriptorType descriptor1("http://some.uri", "1", "id");
  DescriptorType descriptor2(descriptor1);
  EXPECT_EQ("http://some.uri", descriptor2.scheme_id_uri());
  EXPECT_EQ("1", descriptor2.value());
  EXPECT_EQ("id", descriptor2.id());
}

}  // namespace mpd
}  // namespace ndash
