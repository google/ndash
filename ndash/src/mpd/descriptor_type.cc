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

#include "descriptor_type.h"

#include "base/logging.h"

namespace ndash {

namespace mpd {

DescriptorType::DescriptorType(const std::string& scheme_id_uri,
                               const std::string& value,
                               const std::string& id)
    : scheme_id_uri_(scheme_id_uri), value_(value), id_(id) {}

DescriptorType::~DescriptorType() {}

std::string DescriptorType::ToString() const {
  std::string str(scheme_id_uri_);
  str.append(", value=").append(value_).append(", id=").append(id_);
  return str;
}

DescriptorType::DescriptorType(const DescriptorType& other)
    : scheme_id_uri_(other.scheme_id_uri_),
      value_(other.value_),
      id_(other.id_) {}

DescriptorType& DescriptorType::operator=(const DescriptorType& other) {
  scheme_id_uri_ = other.scheme_id_uri_;
  value_ = other.value_;
  id_ = other.id_;
  return *this;
}

}  // namespace mpd

}  // namespace ndash
