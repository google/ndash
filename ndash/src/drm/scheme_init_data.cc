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

#include "base/logging.h"

namespace ndash {

namespace drm {

SchemeInitData::SchemeInitData(const std::string& mime_type,
                               std::unique_ptr<char[]> data,
                               size_t len)
    : mime_type_(mime_type), data_(std::move(data)), len_(len) {
  DCHECK(data_.get() != nullptr);
}

SchemeInitData::SchemeInitData(const SchemeInitData& other)
    : mime_type_(other.mime_type_),
      data_(new char[other.len_]),
      len_(other.len_) {
  memcpy(data_.get(), other.data_.get(), len_);
}

SchemeInitData::~SchemeInitData() {}

bool SchemeInitData::operator==(const SchemeInitData& other) const {
  return mime_type_ == other.mime_type_ && len_ == other.len_ &&
         memcmp(data_.get(), other.data_.get(), len_) == 0;
}

}  // namespace drm

}  // namespace ndash
