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

#include "track_criteria.h"

namespace ndash {

const char kTrickScheme[] = "http://dashif.org/guidelines/trickmode";

TrackCriteria::TrackCriteria(const std::string& mime_type,
                             bool prefer_trick,
                             const std::string& preferred_lang,
                             size_t preferred_channels,
                             const std::string& preferred_codec)
    : mime_type(mime_type),
      prefer_trick(prefer_trick),
      preferred_lang(preferred_lang),
      preferred_channels(preferred_channels),
      preferred_codec(preferred_codec) {}

TrackCriteria::~TrackCriteria() {}

}  // namespace ndash
