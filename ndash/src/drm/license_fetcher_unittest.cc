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

#include "drm/license_fetcher.h"

#include <string>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/synchronization/cancellation_flag.h"
#include "gtest/gtest.h"
#include "upstream/http_data_source.h"
#include "upstream/http_data_source_mock.h"

namespace ndash {

namespace drm {

using ::testing::Eq;
using ::testing::Ge;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::IsNull;
using ::testing::Return;
using ::testing::_;

namespace {
bool CheckLicenseSpec(const upstream::DataSpec& data_spec,
                      const base::CancellationFlag* cancel) {
  // TODO(adewhurst): Check more stuff
  return true;
}
}  // namespace

TEST(LicenseFetcherTests, SuccessfulFetch) {
  const char kUserAgent[] = "ndash LicenseFetcher unittest";
  const char kAuthToken[] = "auth token goes here";
  const std::string kReturnedLicense("here is a license");

  upstream::MockHttpDataSource* data_source =
      new upstream::MockHttpDataSource();

  EXPECT_CALL(*data_source, SetRequestProperty("Content-Type", _));
  EXPECT_CALL(*data_source, SetRequestProperty("User-Agent", kUserAgent));
  EXPECT_CALL(*data_source, SetRequestProperty("Authorization", kAuthToken));

  {
    InSequence seq;
    EXPECT_CALL(*data_source, Open(_, IsNull()))
        .WillOnce(Invoke(&CheckLicenseSpec));
    EXPECT_CALL(*data_source, ReadAllToString(Ge(kReturnedLicense.size())))
        .WillOnce(Return(kReturnedLicense));
    EXPECT_CALL(*data_source, Close());
  }

  LicenseFetcher license_fetcher(base::WrapUnique(data_source), kUserAgent);
  license_fetcher.UpdateAuthToken(kAuthToken);
  license_fetcher.UpdateLicenseUri(
      upstream::Uri("https://gvsb.e2e.gfsvc.com/cenc"));

  std::string dest;
  EXPECT_THAT(license_fetcher.Fetch("payload", &dest), Eq(true));
  EXPECT_THAT(dest, Eq(kReturnedLicense));
}

}  // namespace drm

}  // namespace ndash
