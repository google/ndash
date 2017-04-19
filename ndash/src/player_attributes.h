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

#ifndef NDASH_PLAYER_ATTRIBUTES_H_
#define NDASH_PLAYER_ATTRIBUTES_H_

#include <string>

namespace ndash {

// A container for player attributes.
class PlayerAttributes {
 public:
  PlayerAttributes() {}
  ~PlayerAttributes() {}

  // Authentication header used for out-bound HTTP requests.
  std::string auth_token;
  // The license server url from which to fetch playback licenses.
  std::string license_url;
};

}  // namespace ndash

#endif  // NDASH_PLAYER_ATTRIBUTES_H_
