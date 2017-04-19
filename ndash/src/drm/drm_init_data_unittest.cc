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

#include <memory>

#include "drm/drm_init_data.h"
#include "drm/drm_init_data_mock.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace ndash {
namespace drm {

using ::testing::ByRef;
using ::testing::Eq;
using ::testing::IsNull;
using ::testing::Pointee;

namespace {
std::unique_ptr<SchemeInitData> GenerateSchemeInitData(char base, size_t len) {
  std::unique_ptr<char[]> data(new char[len]);

  char v = base;
  for (size_t i = 0; i < len; i++, v++) {
    data[i] = v;
  }

  std::unique_ptr<SchemeInitData> generated(
      new SchemeInitData("application/octet-stream", std::move(data), len));
  return generated;
}
}  // namespace

TEST(DrmInitDataTest, CanInstantiateMock) {
  scoped_refptr<MockDrmInitData> drm_init_data(new MockDrmInitData);
}

TEST(DrmInitDataTest, UniversalNull) {
  std::unique_ptr<SchemeInitData> null_data;
  UniversalDrmInitData* universal =
      new UniversalDrmInitData(std::move(null_data));
  scoped_refptr<RefCountedDrmInitData> data(universal);

  EXPECT_THAT(universal->Get(util::Uuid()), IsNull());
  EXPECT_THAT(
      universal->Get(util::Uuid::Parse("09514A5C-F8EB-4B5F-B0C3-97F52B47AE8A")),
      IsNull());
}

TEST(DrmInitDataTest, UniversalNotNull) {
  constexpr size_t kTestDataSize = 8;
  std::unique_ptr<SchemeInitData> init_data(
      GenerateSchemeInitData(1, kTestDataSize));
  const SchemeInitData* init_data_ptr = init_data.get();
  UniversalDrmInitData* universal =
      new UniversalDrmInitData(std::move(init_data));
  scoped_refptr<RefCountedDrmInitData> data(universal);

  EXPECT_THAT(universal->Get(util::Uuid()), Eq(init_data_ptr));
  EXPECT_THAT(
      universal->Get(util::Uuid::Parse("09514A5C-F8EB-4B5F-B0C3-97F52B47AE8A")),
      Eq(init_data_ptr));
}

TEST(DrmInitDataTest, Mapped) {
  const util::Uuid null_uuid;
  const util::Uuid uuid1("09514A5C-F8EB-4B5F-B0C3-97F52B47AE8A");
  const util::Uuid uuid2("01234567-89AB-CDEF-0123-456789ABCDEF");
  const util::Uuid uuid3("FEDCBA98-7654-3210-0123-456789ABCDEF");

  std::unique_ptr<SchemeInitData> null_data;

  constexpr size_t kTestDataSize = 8;
  std::unique_ptr<SchemeInitData> init_data1(
      GenerateSchemeInitData(1, kTestDataSize));
  const SchemeInitData* init_data1_ptr = init_data1.get();
  std::unique_ptr<SchemeInitData> init_data2(
      GenerateSchemeInitData(10, kTestDataSize));
  const SchemeInitData* init_data2_ptr = init_data2.get();

  MappedDrmInitData* mapped = new MappedDrmInitData;
  scoped_refptr<RefCountedDrmInitData> data(mapped);

  EXPECT_THAT(mapped->Get(null_uuid), IsNull());
  EXPECT_THAT(mapped->Get(uuid1), IsNull());
  EXPECT_THAT(mapped->Get(uuid2), IsNull());
  EXPECT_THAT(mapped->Get(uuid3), IsNull());

  mapped->Put(uuid1, std::move(init_data1));
  mapped->Put(uuid2, std::move(init_data2));
  mapped->Put(uuid3, std::move(null_data));

  EXPECT_THAT(mapped->Get(null_uuid), IsNull());
  EXPECT_THAT(mapped->Get(uuid1), Eq(init_data1_ptr));
  EXPECT_THAT(mapped->Get(uuid2), Eq(init_data2_ptr));
  EXPECT_THAT(mapped->Get(uuid3), IsNull());
}

}  // namespace drm
}  // namespace ndash
