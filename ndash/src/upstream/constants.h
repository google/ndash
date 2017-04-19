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

#ifndef NDASH_UPSTREAM_CONSTANTS_H_
#define NDASH_UPSTREAM_CONSTANTS_H_

namespace ndash {
namespace upstream {

enum DashplayerUpstreamConstants {
  // A bunch of the MPD code expects -1 to mean unbounded for range lengths
  // TODO(adewhurst): Track down magic numbers and turn them into constants.
  // Also, organize these constants better.
  LENGTH_UNBOUNDED = -1,
  RESULT_IO_ERROR = -2,
  RESULT_END_OF_INPUT = -3,
  RESULT_CONTINUE = -4,
};

constexpr int kBitsPerByte = 8;

}  // namespace upstream
}  // namespace ndash

#endif  // NDASH_UPSTREAM_CONSTANTS_H_
