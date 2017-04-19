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

#include "mpd/adaptation_set.h"

#include <string>

#include "gtest/gtest.h"
#include "mpd/mpd_unittest_helper.h"

namespace ndash {

namespace mpd {

TEST(AdaptationSetTests, NoContentProtections) {
  std::vector<std::unique_ptr<Representation>> representations;

  std::vector<std::unique_ptr<DescriptorType>> supplemental_properties;
  std::unique_ptr<DescriptorType> supplemental(
      new DescriptorType("scheme1", "value1", "id1"));
  supplemental_properties.push_back(std::move(supplemental));
  std::vector<std::unique_ptr<DescriptorType>> essential_properties;

  std::unique_ptr<DescriptorType> essential1(
      new DescriptorType("scheme1", "value1", "id1"));
  std::unique_ptr<DescriptorType> essential2(
      new DescriptorType("scheme2", "value2", "id2"));
  essential_properties.push_back(std::move(essential1));
  essential_properties.push_back(std::move(essential2));

  AdaptationSet adaptation_set(0, AdaptationType::VIDEO, &representations,
                               nullptr, nullptr, &supplemental_properties,
                               &essential_properties);

  EXPECT_TRUE(adaptation_set.HasContentProtection() == false);
  ASSERT_TRUE(adaptation_set.GetContentProtections() != nullptr);
  EXPECT_EQ(0, adaptation_set.GetContentProtections()->size());
  EXPECT_EQ(0, adaptation_set.GetRepresentations()->size());
  EXPECT_EQ(AdaptationType::VIDEO, adaptation_set.GetType());
  EXPECT_EQ(0, adaptation_set.GetId());
  EXPECT_EQ(1, adaptation_set.GetSupplementalPropertyCount());
  EXPECT_EQ(2, adaptation_set.GetEssentialPropertyCount());
}

TEST(AdaptationSetTests, WithContentProtections) {
  std::vector<std::unique_ptr<Representation>> representations;
  std::vector<std::unique_ptr<ContentProtection>> content_protections;

  AdaptationSet adaptation_set(0, AdaptationType::VIDEO, &representations,
                               &content_protections);

  EXPECT_TRUE(adaptation_set.HasContentProtection() == false);
  ASSERT_TRUE(adaptation_set.GetContentProtections() != nullptr);
  EXPECT_EQ(0, adaptation_set.GetContentProtections()->size());
  EXPECT_EQ(0, adaptation_set.GetRepresentations()->size());
  EXPECT_EQ(AdaptationType::VIDEO, adaptation_set.GetType());
  EXPECT_EQ(0, adaptation_set.GetId());
}

TEST(AdaptationSetTests, HasContentProtectionReturnsTrue) {
  std::vector<std::unique_ptr<Representation>> representations;
  std::vector<std::unique_ptr<ContentProtection>> content_protections;

  std::string mime_type = "widevine";
  util::Uuid uuid("09514A5C-F8EB-4B5F-B0C3-97F52B47AE8A");
  size_t length = 10;
  // We don't need real DRM data for this test.
  std::unique_ptr<char[]> data = std::unique_ptr<char[]>(new char[16]);
  memset(data.get(), 0, 16 * sizeof(char));

  std::unique_ptr<drm::SchemeInitData> scheme_init =
      std::unique_ptr<drm::SchemeInitData>(
          new drm::SchemeInitData(mime_type, std::move(data), length));

  std::string scheme = "https://gvsb.e2e.gfsvc.com/cenc";
  std::unique_ptr<ContentProtection> content_protection =
      std::unique_ptr<ContentProtection>(
          new ContentProtection(scheme, uuid, std::move(scheme_init)));
  content_protections.push_back(std::move(content_protection));

  AdaptationSet adaptation_set(0, AdaptationType::VIDEO, &representations,
                               &content_protections);

  EXPECT_TRUE(adaptation_set.HasContentProtection() == true);
}

TEST(AdaptationSetTests, TakesOwnershipOfSegmentBaseIfProvided) {
  std::unique_ptr<SingleSegmentBase> ssb = CreateTestSingleSegmentBase();
  std::vector<std::unique_ptr<Representation>> representations;
  AdaptationSet adaptation_set(0, AdaptationType::VIDEO, &representations,
                               nullptr, std::move(ssb));
  EXPECT_TRUE(adaptation_set.GetSegmentBase() != nullptr);
}

}  // namespace mpd

}  // namespace ndash
