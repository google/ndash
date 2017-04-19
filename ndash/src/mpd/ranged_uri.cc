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

#include "ranged_uri.h"

#include "base/logging.h"
#include "util/uri_util.h"

namespace ndash {

namespace mpd {

RangedUri::RangedUri(const std::string* base_uri,
                     const std::string& reference_uri,
                     int64_t start,
                     int32_t length)
    : base_uri_(base_uri),
      reference_uri_(reference_uri),
      start_(start),
      length_(length) {
  DCHECK(start >= 0);
  DCHECK(length >= 0 || length == -1);
  DCHECK(base_uri != nullptr);
}

RangedUri::~RangedUri() {
  base_uri_ = nullptr;
}

std::string RangedUri::GetUriString() const {
  return util::UriUtil::Resolve(*base_uri_, reference_uri_);
}

int64_t RangedUri::GetStart() const {
  return start_;
}

int32_t RangedUri::GetLength() const {
  return length_;
}

std::unique_ptr<RangedUri> RangedUri::AttemptMerge(
    const RangedUri* other) const {
  if (other == nullptr || GetUriString() != other->GetUriString()) {
    return nullptr;
  } else if (length_ != -1 && start_ + length_ == other->start_) {
    return std::unique_ptr<RangedUri>(
        new RangedUri(base_uri_, reference_uri_, start_,
                      other->length_ == -1 ? -1 : length_ + other->length_));
  } else if (other->length_ != -1 && other->start_ + other->length_ == start_) {
    return std::unique_ptr<RangedUri>(
        new RangedUri(base_uri_, reference_uri_, other->start_,
                      length_ == -1 ? -1 : other->length_ + length_));
  } else {
    return nullptr;
  }
}

bool RangedUri::operator==(const RangedUri& other) const {
  return start_ == other.start_ && length_ == other.length_ &&
         GetUriString() == other.GetUriString();
}

RangedUri& RangedUri::operator=(const RangedUri& other) {
  start_ = other.start_;
  length_ = other.length_;
  base_uri_ = other.base_uri_;
  reference_uri_ = other.reference_uri_;
  return *this;
}

}  // namespace mpd

}  // namespace ndash
