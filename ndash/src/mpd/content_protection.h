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

#ifndef NDASH_MPD_CONTENT_PROTECTION_
#define NDASH_MPD_CONTENT_PROTECTION_

#include <memory>
#include <string>

#include "drm/scheme_init_data.h"
#include "util/uuid.h"

namespace ndash {

namespace mpd {

class ContentProtection {
 public:
  ContentProtection(const std::string& scheme_uri_id,
                    const util::Uuid& uuid,
                    std::unique_ptr<drm::SchemeInitData> data);
  ~ContentProtection();
  bool operator==(const ContentProtection& other) const;
  bool operator!=(const ContentProtection& other) const;

  const drm::SchemeInitData* GetSchemeInitData() const { return data_.get(); }

  const std::string& GetSchemeUriId() const;
  const util::Uuid& GetUuid() const;

 private:
  // Identifies the content protection scheme.
  std::string scheme_uri_id_;

  // The UUID of the protection scheme. May be empty.
  util::Uuid uuid_;

  // Protection scheme specific initialization data. May be null.
  // TODO(rmrossi): Confirm we can take ownership of this.
  std::unique_ptr<drm::SchemeInitData> data_;
};

}  // namespace mpd

}  // namespace ndash

#endif  // NDASH_MPD_CONTENT_PROTECTION_
