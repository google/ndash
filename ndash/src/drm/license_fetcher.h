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

#ifndef NDASH_DRM_LICENSE_FETCHER_H_
#define NDASH_DRM_LICENSE_FETCHER_H_

#include <memory>
#include <string>

#include "base/synchronization/lock.h"
#include "upstream/uri.h"

namespace ndash {

namespace upstream {
class HttpDataSourceInterface;
}  // namespace upstream

namespace drm {

// A utility class to fetch playback licenses synchronously on the calling
// thread.
class LicenseFetcher {
 public:
  LicenseFetcher(std::unique_ptr<upstream::HttpDataSourceInterface> data_source,
                 const std::string& user_agent = "ndash LicenseFetcher");
  ~LicenseFetcher();

  void UpdateLicenseUri(const upstream::Uri& license_uri);
  void UpdateAuthToken(const std::string& auth_token);

  // Make a synchronous request for a playback license.
  // key_message : The body of the POST (constructed by the CDM)
  // out_license : A pointer to a string where the license data should be
  //               stored.  Cannot be null.
  // Returns true upon success, false otherwise in which case the contents of
  // out_license is undefined.
  bool Fetch(const std::string& key_message, std::string* out_license);

 private:
  base::Lock attributes_lock_;
  upstream::Uri license_uri_;
  std::string auth_token_;

  base::Lock fetch_lock_;
  const std::unique_ptr<upstream::HttpDataSourceInterface> data_source_;
};

}  // namespace drm
}  // namespace ndash

#endif  // NDASH_DRM_LICENSE_FETCHER_H_
