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

#include <memory>
#include <string>

#include "upstream/constants.h"
#include "upstream/data_spec.h"
#include "upstream/http_data_source.h"

namespace {
constexpr size_t kBufSize = 8192;
}

namespace ndash {
namespace drm {

LicenseFetcher::LicenseFetcher(
    std::unique_ptr<upstream::HttpDataSourceInterface> data_source,
    const std::string& user_agent)
    : license_uri_(""), data_source_(std::move(data_source)) {
  data_source_->SetRequestProperty("Content-Type", "text/xml;charset=utf=8");
  if (!user_agent.empty()) {
    data_source_->SetRequestProperty("User-Agent", user_agent);
  }
}

LicenseFetcher::~LicenseFetcher() {}

void LicenseFetcher::UpdateLicenseUri(const upstream::Uri& license_uri) {
  base::AutoLock attributes_autolock(attributes_lock_);
  license_uri_ = license_uri;
}

void LicenseFetcher::UpdateAuthToken(const std::string& auth_token) {
  base::AutoLock attributes_autolock(attributes_lock_);
  auth_token_ = auth_token;
}

bool LicenseFetcher::Fetch(const std::string& key_message,
                           std::string* out_license) {
  DCHECK(out_license != nullptr);
  base::AutoLock fetch_autolock(fetch_lock_);

  upstream::Uri license_uri("");
  {
    base::AutoLock attributes_autolock(attributes_lock_);
    license_uri = license_uri_;
    data_source_->SetRequestProperty("Authorization", auth_token_);
  }

  upstream::DataSpec license_spec(license_uri, &key_message, 0, 0,
                                  upstream::LENGTH_UNBOUNDED, nullptr, 0);

  // TODO (rmrossi): Need a way to set connect and read timeout.
  bool open_ok = data_source_->Open(license_spec) != upstream::RESULT_IO_ERROR;

  if (open_ok) {
    *out_license = data_source_->ReadAllToString(kBufSize);
  }

  data_source_->Close();

  return open_ok && !out_license->empty();
}

}  // namespace drm
}  // namespace ndash
