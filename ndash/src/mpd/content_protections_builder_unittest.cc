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

#include "mpd/content_protections_builder.h"

#include <string>

#include "gtest/gtest.h"
#include "mpd/mpd_unittest_helper.h"

namespace ndash {

namespace mpd {

std::unique_ptr<ContentProtection> CreateTestContentProtection(
    std::string scheme,
    std::string uuid_str,
    std::string mime_type) {
  util::Uuid uuid(uuid_str);
  size_t length = 10;
  std::unique_ptr<char[]> data = CreateTestSchemeInitData(length);

  std::unique_ptr<drm::SchemeInitData> scheme_init =
      std::unique_ptr<drm::SchemeInitData>(
          new drm::SchemeInitData(mime_type, std::move(data), length));

  std::unique_ptr<ContentProtection> content_protection(
      new ContentProtection(scheme, uuid, std::move(scheme_init)));
  return content_protection;
}

TEST(ContentProtectionBuilderTests, AdaptationSetOnlyConsistent) {
  ContentProtectionsBuilder builder;

  std::unique_ptr<ContentProtection> cp1 = CreateTestContentProtection(
      "https://gvsb.e2e.gfsvc.com/cenc", "09514A5C-F8EB-4B5F-B0C3-97F52B47AE8A",
      "widevine");

  std::unique_ptr<ContentProtection> cp2 = CreateTestContentProtection(
      "https://gvsb.e2e.gfsvc.com/cenc", "09514A5C-F8EB-4B5F-B0C3-97F52B47AE8A",
      "widevine");

  EXPECT_TRUE(builder.AddAdaptationSetProtection(std::move(cp1)));
  EXPECT_TRUE(builder.AddAdaptationSetProtection(std::move(cp2)));

  std::unique_ptr<ContentProtectionList> final_list = builder.Build();

  ASSERT_TRUE(final_list.get() != nullptr);
  // Only one should survive.
  EXPECT_EQ(1, final_list->size());
}

TEST(ContentProtectionBuilderTests, AdaptationSetOnlyInconsistent) {
  ContentProtectionsBuilder builder;

  std::unique_ptr<ContentProtection> cp1 = CreateTestContentProtection(
      "https://gvsb.e2e.gfsvc.com/cenc", "09514A5C-F8EB-4B5F-B0C3-97F52B47AE8A",
      "widevine");

  std::unique_ptr<ContentProtection> cp2 = CreateTestContentProtection(
      "https://gvsb.e2e.gfsvc.com/cenc", "DEADBEEF-F8EB-4B5F-B0C3-97F52B47AE8A",
      "widevine");

  EXPECT_TRUE(builder.AddAdaptationSetProtection(std::move(cp1)));
  // Different but same scheme is inconsistent.
  EXPECT_FALSE(builder.AddAdaptationSetProtection(std::move(cp2)));
}

TEST(ContentProtectionBuilderTests, AdaptationSetOnlyConsistent2) {
  ContentProtectionsBuilder builder;

  std::unique_ptr<ContentProtection> cp1 = CreateTestContentProtection(
      "https://gvsb.e2e.gfsvc.com/cenc", "09514A5C-F8EB-4B5F-B0C3-97F52B47AE8A",
      "widevine");

  std::unique_ptr<ContentProtection> cp2 = CreateTestContentProtection(
      "https://not.the.same.com/cenc", "DEADBEEF-F8EB-4B5F-B0C3-97F52B47AE8A",
      "widevine");

  EXPECT_TRUE(builder.AddAdaptationSetProtection(std::move(cp1)));
  EXPECT_TRUE(builder.AddAdaptationSetProtection(std::move(cp2)));

  std::unique_ptr<ContentProtectionList> final_list = builder.Build();

  ASSERT_TRUE(final_list.get() != nullptr);
  //  Both should survive since they are different and different schemes.
  EXPECT_EQ(2, final_list->size());
}

TEST(ContentProtectionBuilderTests, RepresentationsOnlyConsistent) {
  ContentProtectionsBuilder builder;

  std::unique_ptr<ContentProtection> cp1 = CreateTestContentProtection(
      "https://gvsb.e2e.gfsvc.com/cenc", "09514A5C-F8EB-4B5F-B0C3-97F52B47AE8A",
      "widevine");

  std::unique_ptr<ContentProtection> cp2 = CreateTestContentProtection(
      "https://gvsb.e2e.gfsvc.com/cenc", "09514A5C-F8EB-4B5F-B0C3-97F52B47AE8A",
      "widevine");

  EXPECT_TRUE(builder.AddRepresentationProtection(std::move(cp1)));
  EXPECT_TRUE(builder.AddRepresentationProtection(std::move(cp2)));
  EXPECT_TRUE(builder.EndRepresentation());

  std::unique_ptr<ContentProtectionList> final_list = builder.Build();

  ASSERT_TRUE(final_list.get() != nullptr);
  // Only one should survive.
  EXPECT_EQ(1, final_list->size());
}

TEST(ContentProtectionBuilderTests, RepresentationsOnlyInconsistent) {
  ContentProtectionsBuilder builder;

  std::unique_ptr<ContentProtection> cp1 = CreateTestContentProtection(
      "https://gvsb.e2e.gfsvc.com/cenc", "09514A5C-F8EB-4B5F-B0C3-97F52B47AE8A",
      "widevine");

  std::unique_ptr<ContentProtection> cp2 = CreateTestContentProtection(
      "https://gvsb.e2e.gfsvc.com/cenc", "DEADBEEF-F8EB-4B5F-B0C3-97F52B47AE8A",
      "widevine");

  EXPECT_TRUE(builder.AddRepresentationProtection(std::move(cp1)));
  // Different but same scheme is inconsistent.
  EXPECT_FALSE(builder.AddRepresentationProtection(std::move(cp2)));
}

TEST(ContentProtectionBuilderTests, RepresentationsOnlyConsistent2) {
  ContentProtectionsBuilder builder;

  std::unique_ptr<ContentProtection> cp1 = CreateTestContentProtection(
      "https://gvsb.e2e.gfsvc.com/cenc", "09514A5C-F8EB-4B5F-B0C3-97F52B47AE8A",
      "widevine");

  std::unique_ptr<ContentProtection> cp2 = CreateTestContentProtection(
      "https://not.the.same.com/cenc", "DEADBEEF-F8EB-4B5F-B0C3-97F52B47AE8A",
      "widevine");

  EXPECT_TRUE(builder.AddRepresentationProtection(std::move(cp1)));
  EXPECT_TRUE(builder.AddRepresentationProtection(std::move(cp2)));
  EXPECT_TRUE(builder.EndRepresentation());

  std::unique_ptr<ContentProtectionList> final_list = builder.Build();

  ASSERT_TRUE(final_list.get() != nullptr);
  //  Both should survive since they are different and different schemes.
  EXPECT_EQ(2, final_list->size());
}

TEST(ContentProtectionBuilderTests, AdaptationSetAndRepresentationsConsistent) {
  ContentProtectionsBuilder builder;

  // Simulate AdaptationSet
  std::unique_ptr<ContentProtection> cp1 = CreateTestContentProtection(
      "https://gvsb.e2e.gfsvc.com/cenc", "09514A5C-F8EB-4B5F-B0C3-97F52B47AE8A",
      "widevine");
  EXPECT_TRUE(builder.AddAdaptationSetProtection(std::move(cp1)));

  // Simulate representation 1
  std::unique_ptr<ContentProtection> cp2 = CreateTestContentProtection(
      "https://gvsb.e2e.gfsvc.com/cenc", "09514A5C-F8EB-4B5F-B0C3-97F52B47AE8A",
      "widevine");
  EXPECT_TRUE(builder.AddRepresentationProtection(std::move(cp2)));

  std::unique_ptr<ContentProtection> cp3 = CreateTestContentProtection(
      "https://gvsb.e2e.gfsvc.com/cenc", "09514A5C-F8EB-4B5F-B0C3-97F52B47AE8A",
      "widevine");
  EXPECT_TRUE(builder.AddRepresentationProtection(std::move(cp3)));
  EXPECT_TRUE(builder.EndRepresentation());

  // Simulate representation 2
  std::unique_ptr<ContentProtection> cp4 = CreateTestContentProtection(
      "https://gvsb.e2e.gfsvc.com/cenc", "09514A5C-F8EB-4B5F-B0C3-97F52B47AE8A",
      "widevine");
  EXPECT_TRUE(builder.AddRepresentationProtection(std::move(cp4)));

  std::unique_ptr<ContentProtection> cp5 = CreateTestContentProtection(
      "https://gvsb.e2e.gfsvc.com/cenc", "09514A5C-F8EB-4B5F-B0C3-97F52B47AE8A",
      "widevine");
  EXPECT_TRUE(builder.AddRepresentationProtection(std::move(cp5)));
  EXPECT_TRUE(builder.EndRepresentation());

  std::unique_ptr<ContentProtectionList> final_list = builder.Build();

  ASSERT_TRUE(final_list.get() != nullptr);
  // Only one should survive. All consistent.
  EXPECT_EQ(1, final_list->size());
}

TEST(ContentProtectionBuilderTests,
     AdaptationSetAndRepresentationsInconsistent) {
  ContentProtectionsBuilder builder;

  // Simulate AdaptationSet
  std::unique_ptr<ContentProtection> cp1 = CreateTestContentProtection(
      "https://gvsb.e2e.gfsvc.com/cenc", "09514A5C-F8EB-4B5F-B0C3-97F52B47AE8A",
      "widevine");
  EXPECT_TRUE(builder.AddAdaptationSetProtection(std::move(cp1)));

  // Simulate representation 1
  std::unique_ptr<ContentProtection> cp2 = CreateTestContentProtection(
      "https://gvsb.e2e.gfsvc.com/cenc", "DEADBEEF-F8EB-4B5F-B0C3-97F52B47AE8A",
      "widevine");
  // Still okay since we haven't tried to build the final list yet.
  EXPECT_TRUE(builder.AddRepresentationProtection(std::move(cp2)));
  EXPECT_TRUE(builder.EndRepresentation());

  // Should get nullptr due to inconsistency.
  std::unique_ptr<ContentProtectionList> final_list = builder.Build();
  EXPECT_EQ(nullptr, final_list.get());
}

TEST(ContentProtectionBuilderTests, ResultsAreSorted) {
  ContentProtectionsBuilder builder;

  std::unique_ptr<ContentProtection> cp1 = CreateTestContentProtection(
      "https://zeebra", "09514A5C-F8EB-4B5F-B0C3-97F52B47AE8A", "widevine");

  std::unique_ptr<ContentProtection> cp2 = CreateTestContentProtection(
      "https://yak", "09514A5C-F8EB-4B5F-B0C3-97F52B47AE8B", "widevine");

  std::unique_ptr<ContentProtection> cp3 = CreateTestContentProtection(
      "https://bobcat", "09514A5C-F8EB-4B5F-B0C3-97F52B47AE8C", "widevine");

  std::unique_ptr<ContentProtection> cp4 = CreateTestContentProtection(
      "https://anteater", "09514A5C-F8EB-4B5F-B0C3-97F52B47AE8D", "widevine");

  EXPECT_TRUE(builder.AddRepresentationProtection(std::move(cp1)));
  EXPECT_TRUE(builder.AddRepresentationProtection(std::move(cp2)));
  EXPECT_TRUE(builder.AddRepresentationProtection(std::move(cp3)));
  EXPECT_TRUE(builder.AddRepresentationProtection(std::move(cp4)));
  EXPECT_TRUE(builder.EndRepresentation());

  std::unique_ptr<ContentProtectionList> final_list = builder.Build();

  ASSERT_TRUE(final_list.get() != nullptr);
  EXPECT_EQ(4, final_list->size());
  EXPECT_EQ("https://anteater", final_list->at(0)->GetSchemeUriId());
  EXPECT_EQ("https://bobcat", final_list->at(1)->GetSchemeUriId());
  EXPECT_EQ("https://yak", final_list->at(2)->GetSchemeUriId());
  EXPECT_EQ("https://zeebra", final_list->at(3)->GetSchemeUriId());
}

}  // namespace mpd

}  // namespace ndash
