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

#ifndef NDASH_UTIL_FORMAT_H_
#define NDASH_UTIL_FORMAT_H_

#include <cstdint>
#include <iostream>
#include <string>

namespace ndash {

namespace util {

// Defines the high level format of a media stream.
class Format {
 public:
  Format(const std::string& id,
         const std::string& mime_type,
         int32_t width,
         int32_t height,
         double frame_rate,
         int32_t max_playout_rate,
         int32_t audio_channels,
         int32_t audio_sampling_rate,
         int32_t bitrate,
         const std::string& language = "",
         const std::string& codecs = "");
  Format(const Format& other);
  ~Format();

  // Implements equality based on id only.
  bool operator==(const Format& other) const;

  int32_t GetAudioChannels() const { return audio_channels_; }

  int32_t GetAudioSamplingRate() const { return audio_sampling_rate_; }

  int32_t GetBitrate() const { return bitrate_; }

  const std::string& GetCodecs() const { return codecs_; }

  double GetFrameRate() const { return frame_rate_; }

  int32_t GetMaxPlayoutRate() const { return max_playout_rate_; }

  int32_t GetHeight() const { return height_; }

  const std::string& GetId() const { return id_; }

  const std::string& GetLanguage() const { return language_; }

  const std::string& GetMimeType() const { return mime_type_; }

  const int32_t GetWidth() const { return width_; }

  class DecreasingBandwidthComparator {
   public:
    bool operator()(const Format& lhs, const Format& rhs) const {
      return lhs.GetBitrate() > rhs.GetBitrate();
    }
  };

 private:
  // An identifier for the format.
  std::string id_;

  // The mime type of the format.
  std::string mime_type_;

  // The width of the video in pixels, or -1 if unknown or not applicable.
  int32_t width_;

  // The height of the video in pixels, or -1 if unknown or not applicable.
  int32_t height_;

  // The video frame rate in frames per second, or -1 if unknown or not
  // applicable.
  double frame_rate_;

  // The maximum playout rate as a multiple of the regular playout rate.
  int32_t max_playout_rate_;

  // The number of audio channels, or -1 if unknown or not applicable.
  int32_t audio_channels_;

  // The audio sampling rate in Hz, or -1 if unknown or not applicable.
  int32_t audio_sampling_rate_;

  // The average bandwidth in bits per second.
  int32_t bitrate_;

  // The language of the format. Can be empty if unknown.
  // The language codes are two-letter lowercase ISO language codes
  // (such as "en") as defined by ISO 639-1.
  std::string language_;

  // The codecs used to decode the format. Can be empty if unknown.
  std::string codecs_;
};

// gmock pretty printer
void PrintTo(const Format& format, ::std::ostream* os);

}  // namespace util

}  // namespace ndash

#endif  // NDASH_UTIL_FORMAT_H_
