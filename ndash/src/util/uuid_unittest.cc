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

#include "util/uuid.h"

#include "gtest/gtest.h"

namespace ndash {

namespace util {

TEST(UuidTests, EmptyUuid) {
  util::Uuid uuid;

  std::string asString;
  uuid.ToString(&asString);

  EXPECT_EQ("00000000-0000-0000-0000-000000000000", asString);
  EXPECT_TRUE(uuid.is_empty());
}

TEST(UuidTests, UuidFromString) {
  util::Uuid uuid("09514A5C-F8EB-4B5F-B0C3-97F52B47AE8A");

  std::string asString;
  uuid.ToString(&asString);

  EXPECT_EQ("09514A5C-F8EB-4B5F-B0C3-97F52B47AE8A", asString);
}

TEST(UuidTests, UuidEquivalence) {
  util::Uuid uuid("09514A5C-F8EB-4B5F-B0C3-97F52B47AE8A");
  util::Uuid uuid2(uuid);

  EXPECT_EQ(uuid, uuid2);
}

TEST(UuidTests, UuidBad) {
  util::Uuid uuid("fail");

  EXPECT_TRUE(uuid.is_empty());
}

TEST(UuidTests, UuidMostSignificantBits) {
  util::Uuid uuid("09514A5C-F8EB-4B5F-B0C3-97F52B47AE8A");

  uint64_t val = uuid.most_significant_bits();
  EXPECT_EQ(0x9514A5CF8EB4B5F, val);
}

TEST(UuidTests, UuidLeastSignificantBits) {
  util::Uuid uuid("09514A5C-F8EB-4B5F-B0C3-97F52B47AE8A");

  uint64_t val = uuid.least_significant_bits();
  EXPECT_EQ(0xB0C397F52B47AE8A, val);
}

}  // namespace util

}  // namespace ndash
