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

#ifndef NDASH_UTIL_MIME_TYPES_H_
#define NDASH_UTIL_MIME_TYPES_H_

#include <string>

#include "base/strings/string_piece.h"

namespace ndash {

namespace util {

extern const char kBaseTypeVideo[];
extern const char kBaseTypeAudio[];
extern const char kBaseTypeText[];
extern const char kBaseTypeApplication[];

extern const char kVideoUnknown[];
extern const char kVideoMP4[];
extern const char kVideoWebM[];
extern const char kVideoH263[];
extern const char kVideoH264[];
extern const char kVideoH265[];
extern const char kVideoVP8[];
extern const char kVideoVP9[];
extern const char kVideoMP4V[];
extern const char kVideoMPEG2[];
extern const char kVideoVC1[];

extern const char kAudioUnknown[];
extern const char kAudioMP4[];
extern const char kAudioAAC[];
extern const char kAudioWebM[];
extern const char kAudioMPEG[];
extern const char kAudioMPEG_L1[];
extern const char kAudioMPEG_L2[];
extern const char kAudioRaw[];
extern const char kAudioAC3[];
extern const char kAudioE_AC3[];
extern const char kAudioTrueHD[];
extern const char kAudioDTS[];
extern const char kAudioDTS_HD[];
extern const char kAudioDTS_Express[];
extern const char kAudioVorbis[];
extern const char kAudioOpus[];
extern const char kAudioAMR_NB[];
extern const char kAudioAMR_WB[];
extern const char kAudioFLAC[];

extern const char kTextUnknown[];
extern const char kTextVTT[];

extern const char kApplicationMP4[];
extern const char kApplicationWebM[];
extern const char kApplicationID3[];
extern const char kApplicationEIA608[];
extern const char kApplicationSubRip[];
extern const char kApplicationTTML[];
extern const char kApplicationM3U8[];
extern const char kApplicationTX3G[];
extern const char kApplicationMP4VTT[];
extern const char kApplicationVOBsub[];
extern const char kApplicationPGS[];
extern const char kApplicationRAWCC[];

class MimeTypes {
 public:
  // Returns whether the top-level type of mime_type is application.
  static bool IsApplication(const std::string& mime_type);
  // Returns whether mime_type maps to a known TEXT content type.
  static bool IsText(const std::string& mime_type);
  // Returns whether the top-level type of mime_type is audio.
  static bool IsAudio(const std::string& mime_type);
  // Returns whether the top-level type of mime_type is video.
  static bool IsVideo(const std::string& mime_type);

  // Returns the top-level type of the provided mime_type in output.
  // Returns true for success, false if input is not valid.
  static bool GetTopLevelType(const std::string& mime_type,
                              base::StringPiece* output);
  // Returns the sub-type of the provided mime_type in output.
  // Returns true for success, false if input is not valid.
  static bool GetSubType(const std::string& mime_type,
                         base::StringPiece* output);

  static const char* GetVideoMediaMimeType(const std::string& codecs);
  static const char* GetAudioMediaMimeType(const std::string& codecs);

 private:
  MimeTypes() = delete;
};

}  // namespace util

}  // namespace ndash

#endif  // NDASH_UTIL_MIME_TYPES_H_
