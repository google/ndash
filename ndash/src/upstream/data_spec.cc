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

#include "upstream/data_spec.h"

#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

#include "base/logging.h"
#include "upstream/constants.h"

namespace ndash {
namespace upstream {

DataSpec::DataSpec(const Uri& uri) : DataSpec(uri, 0) {}

DataSpec::DataSpec(const Uri& uri, int flags)
    : DataSpec(uri, 0, LENGTH_UNBOUNDED, nullptr, flags) {}

DataSpec::DataSpec(const Uri& uri,
                   int64_t absolute_stream_position,
                   int64_t length,
                   const std::string* key)
    : DataSpec(uri,
               absolute_stream_position,
               absolute_stream_position,
               length,
               key,
               0) {}

DataSpec::DataSpec(const Uri& uri,
                   int64_t absolute_stream_position,
                   int64_t length,
                   const std::string* key,
                   int flags)
    : DataSpec(uri,
               absolute_stream_position,
               absolute_stream_position,
               length,
               key,
               flags) {}

DataSpec::DataSpec(const Uri& uri,
                   int64_t absolute_stream_position,
                   int64_t position,
                   int64_t length,
                   const std::string* key,
                   int flags)
    : DataSpec(uri,
               nullptr,
               absolute_stream_position,
               position,
               length,
               key,
               flags) {}

DataSpec::DataSpec(const Uri& uri,
                   const std::string* post_body,
                   int64_t absolute_stream_position,
                   int64_t position,
                   int64_t length,
                   const std::string* key,
                   int flags)
    : uri(uri.uri()),
      post_body(post_body ? new std::string(*post_body) : nullptr),
      absolute_stream_position(absolute_stream_position),
      position(position),
      length(length),
      key(key ? new std::string(*key) : nullptr),
      flags(flags) {
  DCHECK(absolute_stream_position >= 0);
  DCHECK(position >= 0);
  DCHECK(length > 0 || length == LENGTH_UNBOUNDED);
}

DataSpec::DataSpec(const DataSpec& other)
    : uri(other.uri.uri()),
      post_body(other.post_body ? new std::string(*other.post_body) : nullptr),
      absolute_stream_position(other.absolute_stream_position),
      position(other.position),
      length(other.length),
      key(other.key ? new std::string(*other.key) : nullptr),
      flags(other.flags) {}

DataSpec::~DataSpec() {}

std::string DataSpec::DebugString() const {
  std::ostringstream ss;
  ss << "DataSpec[" << uri.uri() << ", "
     << (post_body ? *post_body : std::string("(null)")) << ", "
     << absolute_stream_position << ", " << position << ", ";

  if (length == LENGTH_UNBOUNDED) {
    ss << "UNB";
  } else {
    ss << length;
  }

  ss << ", " << (key ? *key : std::string("(null)")) << ", " << flags << "]";
  return ss.str();
}

DataSpec DataSpec::GetRemainder(const DataSpec& data_spec,
                                int64_t bytes_loaded) {
  return DataSpec(data_spec.uri, data_spec.position + bytes_loaded,
                  data_spec.length == LENGTH_UNBOUNDED
                      ? LENGTH_UNBOUNDED
                      : data_spec.length - bytes_loaded,
                  data_spec.key.get(), data_spec.flags);
}

}  // namespace upstream
}  // namespace ndash
