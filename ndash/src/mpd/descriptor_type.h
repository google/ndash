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

#ifndef NDASH_MPD_DESCRIPTOR_TYPE_
#define NDASH_MPD_DESCRIPTOR_TYPE_

#include <string>

namespace ndash {

namespace mpd {

// DescriptorType as specified by DASH-MPD.xsd
class DescriptorType {
 public:
  DescriptorType(const std::string& scheme_id_uri,
                 const std::string& value,
                 const std::string& id = "");
  ~DescriptorType();
  std::string ToString() const;

  DescriptorType(const DescriptorType& other);

  DescriptorType& operator=(const DescriptorType& other);

  const std::string& scheme_id_uri() const { return scheme_id_uri_; }
  const std::string& value() const { return value_; }
  const std::string& id() const { return id_; }

 private:
  std::string scheme_id_uri_;
  std::string value_;
  std::string id_;
};

}  // namespace mpd

}  // namespace ndash

#endif  // NDASH_MPD_DESCRIPTOR_TYPE_
