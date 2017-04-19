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

#include "util/mime_types.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace ndash {

namespace util {

const char kBaseTypeVideo[] = "video";
const char kBaseTypeAudio[] = "audio";
const char kBaseTypeText[] = "text";
const char kBaseTypeApplication[] = "application";

const char kVideoMp4[] = "video/mp4";

const char kApplicationTtml[] = "application/ttml+xml";
const char kApplicationMp4[] = "application/mp4";

const char kVideoUnknown[] = "video/x-unknown";
const char kVideoMP4[] = "video/mp4";
const char kVideoWebM[] = "video/webm";
const char kVideoH263[] = "video/3gpp";
const char kVideoH264[] = "video/avc";
const char kVideoH265[] = "video/hevc";
const char kVideoVP8[] = "video/x-vnd.on2.vp8";
const char kVideoVP9[] = "video/x-vnd.on2.vp9";
const char kVideoMP4V[] = "video/mp4v-es";
const char kVideoMPEG2[] = "video/mpeg2";
const char kVideoVC1[] = "video/wvc1";

const char kAudioUnknown[] = "audio/x-unknown";
const char kAudioMP4[] = "audio/mp4";
const char kAudioAAC[] = "audio/mp4a-latm";
const char kAudioWebM[] = "audio/webm";
const char kAudioMPEG[] = "audio/mpeg";
const char kAudioMPEG_L1[] = "audio/mpeg-L1";
const char kAudioMPEG_L2[] = "audio/mpeg-L2";
const char kAudioRaw[] = "audio/raw";
const char kAudioAC3[] = "audio/ac3";
const char kAudioE_AC3[] = "audio/eac3";
const char kAudioTrueHD[] = "audio/true-hd";
const char kAudioDTS[] = "audio/vnd.dts";
const char kAudioDTS_HD[] = "audio/vnd.dts.hd";
const char kAudioDTS_Express[] = "audio/vnd.dts.hd;profile=lbr";
const char kAudioVorbis[] = "audio/vorbis";
const char kAudioOpus[] = "audio/opus";
const char kAudioAMR_NB[] = "audio/3gpp";
const char kAudioAMR_WB[] = "audio/amr-wb";
const char kAudioFLAC[] = "audio/x-flac";

const char kTextUnknown[] = "text/x-unknown";
const char kTextVTT[] = "text/vtt";

const char kApplicationMP4[] = "application/mp4";
const char kApplicationWebM[] = "application/webm";
const char kApplicationID3[] = "application/id3";
const char kApplicationEIA608[] = "application/eia-608";
const char kApplicationSubRip[] = "application/x-subrip";
const char kApplicationTTML[] = "application/ttml+xml";
const char kApplicationM3U8[] = "application/x-mpegURL";
const char kApplicationTX3G[] = "application/x-quicktime-tx3g";
const char kApplicationMP4VTT[] = "application/x-mp4vtt";
const char kApplicationVOBsub[] = "application/vobsub";
const char kApplicationPGS[] = "application/pgs";
const char kApplicationRAWCC[] = "application/x-rawcc";

bool MimeTypes::IsApplication(const std::string& mime_type) {
  base::StringPiece top_level;
  if (GetTopLevelType(mime_type, &top_level)) {
    return top_level == kBaseTypeApplication;
  }
  return false;
}

bool MimeTypes::IsText(const std::string& mime_type) {
  return mime_type == kApplicationRAWCC || mime_type == kTextVTT ||
         mime_type == util::kApplicationTTML;
}

bool MimeTypes::IsAudio(const std::string& mime_type) {
  base::StringPiece top_level;
  if (GetTopLevelType(mime_type, &top_level)) {
    return top_level == kBaseTypeAudio;
  }
  return false;
}

bool MimeTypes::IsVideo(const std::string& mime_type) {
  base::StringPiece top_level;
  if (GetTopLevelType(mime_type, &top_level)) {
    return top_level == kBaseTypeVideo;
  }
  return false;
}

bool MimeTypes::GetTopLevelType(const std::string& mime_type,
                                base::StringPiece* output) {
  if (output != nullptr) {
    int index_of_slash = mime_type.find('/');
    if (index_of_slash != std::string::npos) {
      *output = base::StringPiece(mime_type.c_str(), index_of_slash);
      return true;
    }
  }
  return false;
}

bool MimeTypes::GetSubType(const std::string& mime_type,
                           base::StringPiece* output) {
  if (output != nullptr) {
    int index_of_slash = mime_type.find('/');
    if (index_of_slash != std::string::npos) {
      *output = base::StringPiece(mime_type.cbegin() + index_of_slash + 1,
                                  mime_type.cend());
      return true;
    }
  }
  return false;
}

const char* MimeTypes::GetVideoMediaMimeType(const std::string& codecs) {
  for (const auto& codec : base::SplitStringPiece(
           codecs, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    if (base::StartsWith(codec, "avc1", base::CompareCase::SENSITIVE) ||
        base::StartsWith(codec, "avc3", base::CompareCase::SENSITIVE)) {
      return kVideoH264;
    } else if (base::StartsWith(codec, "hev1", base::CompareCase::SENSITIVE) ||
               base::StartsWith(codec, "hvc1", base::CompareCase::SENSITIVE)) {
      return kVideoH265;
    } else if (base::StartsWith(codec, "vp9", base::CompareCase::SENSITIVE)) {
      return kVideoVP9;
    } else if (base::StartsWith(codec, "vp8", base::CompareCase::SENSITIVE)) {
      return kVideoVP8;
    }
  }

  return kVideoUnknown;
}

const char* MimeTypes::GetAudioMediaMimeType(const std::string& codecs) {
  for (const auto& codec : base::SplitStringPiece(
           codecs, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    if (base::StartsWith(codec, "mp4a", base::CompareCase::SENSITIVE)) {
      return kAudioAAC;
    } else if (base::StartsWith(codec, "ac-3", base::CompareCase::SENSITIVE) ||
               base::StartsWith(codec, "dac3", base::CompareCase::SENSITIVE)) {
      return kAudioAC3;
    } else if (base::StartsWith(codec, "ec-3", base::CompareCase::SENSITIVE) ||
               base::StartsWith(codec, "dec3", base::CompareCase::SENSITIVE)) {
      return kAudioE_AC3;
    } else if (base::StartsWith(codec, "dtsc", base::CompareCase::SENSITIVE)) {
      return kAudioDTS;
    } else if (base::StartsWith(codec, "dtsh", base::CompareCase::SENSITIVE) ||
               base::StartsWith(codec, "dts1", base::CompareCase::SENSITIVE)) {
      return kAudioDTS_HD;
    } else if (base::StartsWith(codec, "dtse", base::CompareCase::SENSITIVE)) {
      return kAudioDTS_Express;
    } else if (base::StartsWith(codec, "opus", base::CompareCase::SENSITIVE)) {
      return kAudioOpus;
    } else if (base::StartsWith(codec, "vorbis",
                                base::CompareCase::SENSITIVE)) {
      return kAudioVorbis;
    }
  }

  return kAudioUnknown;
}

}  // namespace util

}  // namespace ndash
