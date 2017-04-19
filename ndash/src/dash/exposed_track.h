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

#ifndef NDASH_DASH_EXPOSED_TRACK_H_
#define NDASH_DASH_EXPOSED_TRACK_H_

#include <cstdint>
#include <memory>

namespace ndash {

class MediaFormat;

namespace dash {

// Internal to DashChunkSource, holds track data
struct ExposedTrack {
  ExposedTrack(std::unique_ptr<MediaFormat> track_format,
               int32_t adaptation_set_index);
  ~ExposedTrack();

  ExposedTrack(const ExposedTrack& other) = delete;
  ExposedTrack& operator=(const ExposedTrack& other) = delete;

  const std::unique_ptr<MediaFormat> track_format;

  const int32_t adaptation_set_index;
};

}  // namespace dash
}  // namespace ndash

#endif  // NDASH_DASH_EXPOSED_TRACK_H_
