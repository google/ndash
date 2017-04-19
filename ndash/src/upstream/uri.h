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

#ifndef NDASH_UPSTREAM_URI_H_
#define NDASH_UPSTREAM_URI_H_

#include <string>

#include "base/strings/string_piece.h"

namespace ndash {
namespace upstream {

// A trivial implementation of a URI class as a placeholder since ExoPlayer
// usually uses Android's URI class
class Uri {
 public:
  explicit Uri(const std::string& uri);
  Uri(const Uri& other);
  ~Uri();

  const std::string& uri() const { return uri_; }
  void set_uri(const std::string& uri) { uri_ = uri; }

  Uri& operator=(const Uri& rhs);

 private:
  std::string uri_;
};

}  // namespace upstream
}  // namespace ndash

#endif  // NDASH_UPSTREAM_URI_H_
