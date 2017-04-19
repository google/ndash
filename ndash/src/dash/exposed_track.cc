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

#include "dash/exposed_track.h"

#include "media_format.h"

namespace ndash {
namespace dash {

ExposedTrack::ExposedTrack(std::unique_ptr<MediaFormat> track_format,
                           int32_t adaptation_set_index)
    : track_format(std::move(track_format)),
      adaptation_set_index(adaptation_set_index) {}

ExposedTrack::~ExposedTrack() {}

}  // namespace dash
}  // namespace ndash
