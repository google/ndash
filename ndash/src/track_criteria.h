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

#ifndef NDASH_TRACK_CRITERIA_H_
#define NDASH_TRACK_CRITERIA_H_

#include <cstdint>
#include <memory>
#include <string>

namespace ndash {

extern const char kTrickScheme[];

// Represents criteria that will aid in selecting one track among a number
// of available tracks provided by a ChunkSource.
//
// For an explanation of how this criteria is used by various ChunkSource
// implementations, please refer to those implementation header files.
struct TrackCriteria {
  TrackCriteria(const std::string& mime_type,
                bool prefer_trick = false,
                const std::string& preferred_lang = "",
                size_t preferred_channels_ = 0,
                const std::string& preferred_codec = "");
  ~TrackCriteria();

  TrackCriteria(const TrackCriteria& other) = delete;
  TrackCriteria& operator=(const TrackCriteria& other) = delete;

  // Only tracks with matching mime type attributes are considered.
  std::string mime_type;

  // If true, tracks marked as trick tracks are preferred.
  bool prefer_trick;

  // If non-empty, tracks with a matching lang attribute are preferred.
  std::string preferred_lang;

  // If > 0, prefer tracks containing audio channels greater than or equal to
  // this number.
  size_t preferred_channels;

  // If non-empty, tracks marked with the given codec are preferred.
  std::string preferred_codec;
};

}  // namespace ndash

#endif  // NDASH_TRACK_CRITERIA_H_
