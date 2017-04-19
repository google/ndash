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

#include <string>

#include "gtest/gtest.h"
#include "mpd/content_protection.h"
#include "mpd/mpd_unittest_helper.h"
#include "util/uuid.h"

namespace ndash {
namespace drm {

TEST(ContentProtectionTests, ContentProtectionTest) {
  std::string mime_type = "widevine";
  util::Uuid uuid("09514A5C-F8EB-4B5F-B0C3-97F52B47AE8A");
  size_t length = 10;
  std::unique_ptr<char[]> data = mpd::CreateTestSchemeInitData(length);

  std::unique_ptr<drm::SchemeInitData> scheme_init =
      std::unique_ptr<drm::SchemeInitData>(
          new drm::SchemeInitData(mime_type, std::move(data), length));

  std::string scheme = "https://gvsb.e2e.gfsvc.com/cenc";
  mpd::ContentProtection content_protection(scheme, uuid,
                                            std::move(scheme_init));

  // Constructor
  EXPECT_NE(nullptr, content_protection.GetSchemeInitData());
  EXPECT_EQ(uuid, content_protection.GetUuid());
  EXPECT_EQ(scheme, content_protection.GetSchemeUriId());

  // Equality
  size_t length2 = 10;
  std::unique_ptr<char[]> data2 = mpd::CreateTestSchemeInitData(length2);
  std::unique_ptr<drm::SchemeInitData> scheme_init2 =
      std::unique_ptr<drm::SchemeInitData>(
          new drm::SchemeInitData(mime_type, std::move(data2), length2));
  mpd::ContentProtection content_protection2(scheme, uuid,
                                             std::move(scheme_init2));
  EXPECT_TRUE(content_protection == content_protection2);

  // Different data same length not equal
  size_t length3 = 10;
  std::unique_ptr<char[]> data3 = mpd::CreateTestSchemeInitData(length3);
  data3[0] = -1;
  std::unique_ptr<drm::SchemeInitData> scheme_init3 =
      std::unique_ptr<drm::SchemeInitData>(
          new drm::SchemeInitData(mime_type, std::move(data3), length3));
  mpd::ContentProtection content_protection3(scheme, uuid,
                                             std::move(scheme_init3));
  EXPECT_FALSE(content_protection == content_protection3);

  // Different length not equal
  size_t length4 = 5;
  std::unique_ptr<char[]> data4 = mpd::CreateTestSchemeInitData(length4);
  std::unique_ptr<drm::SchemeInitData> scheme_init4 =
      std::unique_ptr<drm::SchemeInitData>(
          new drm::SchemeInitData(mime_type, std::move(data4), length4));
  mpd::ContentProtection content_protection4(scheme, uuid,
                                             std::move(scheme_init4));
  EXPECT_FALSE(content_protection == content_protection4);

  // Null data, not equal
  mpd::ContentProtection content_protection5(scheme, uuid, nullptr);
  EXPECT_FALSE(content_protection == content_protection5);
}

}  // namespace drm
}  // namespace ndash
