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

#include "util/format.h"

namespace ndash {

namespace util {

Format::Format(const std::string& id,
               const std::string& mime_type,
               int32_t width,
               int32_t height,
               double frame_rate,
               int32_t max_playout_rate,
               int32_t num_channels,
               int32_t audio_sampling_rate,
               int32_t bitrate,
               const std::string& language,
               const std::string& codecs)
    : id_(id),
      mime_type_(mime_type),
      width_(width),
      height_(height),
      frame_rate_(frame_rate),
      max_playout_rate_(max_playout_rate),
      audio_channels_(num_channels),
      audio_sampling_rate_(audio_sampling_rate),
      bitrate_(bitrate),
      language_(language),
      codecs_(codecs) {}

Format::~Format() {}

Format::Format(const Format& other)
    : id_(other.id_),
      mime_type_(other.mime_type_),
      width_(other.width_),
      height_(other.height_),
      frame_rate_(other.frame_rate_),
      max_playout_rate_(other.max_playout_rate_),
      audio_channels_(other.audio_channels_),
      audio_sampling_rate_(other.audio_sampling_rate_),
      bitrate_(other.bitrate_),
      language_(other.language_),
      codecs_(other.codecs_) {}

bool Format::operator==(const Format& other) const {
  return id_ == other.id_;
}

void PrintTo(const Format& format, ::std::ostream* os) {
  *os << "Format[" << format.GetId() << "; mime=" << format.GetMimeType()
      << "; playout=" << format.GetMaxPlayoutRate()
      << "; bitrate=" << format.GetBitrate() << "]";
}

}  // namespace util

}  // namespace ndash
