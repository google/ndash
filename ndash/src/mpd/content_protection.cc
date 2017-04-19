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

#include "mpd/content_protection.h"

#include "base/logging.h"

namespace ndash {

namespace mpd {

ContentProtection::ContentProtection(const std::string& scheme_uri_id,
                                     const util::Uuid& uuid,
                                     std::unique_ptr<drm::SchemeInitData> data)
    : scheme_uri_id_(scheme_uri_id), uuid_(uuid), data_(std::move(data)) {
  DCHECK(scheme_uri_id.length() > 0);
}

ContentProtection::~ContentProtection() {}

const std::string& ContentProtection::GetSchemeUriId() const {
  return scheme_uri_id_;
}

const util::Uuid& ContentProtection::GetUuid() const {
  return uuid_;
}

bool ContentProtection::operator==(const ContentProtection& other) const {
  bool data_equal =
      (data_ == nullptr && other.data_ == nullptr) ||
      (data_ != nullptr && other.data_ != nullptr && *data_ == *other.data_);
  return data_equal && scheme_uri_id_ == other.scheme_uri_id_ &&
         uuid_ == other.uuid_;
}

bool ContentProtection::operator!=(const ContentProtection& other) const {
  return !(*this == other);
}

}  // namespace mpd

}  // namespace ndash
