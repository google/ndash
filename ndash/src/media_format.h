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

#ifndef NDASH_MEDIA_FORMAT_H_
#define NDASH_MEDIA_FORMAT_H_

#include <cstdint>
#include <limits>
#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "ndash.h"

namespace ndash {

constexpr int kNoValue = -1;

// A value for subsample_offset_us to indicate that subsample timestamps are
// relative to the timestamps of their parent samples.
constexpr int64_t kOffsetSampleRelative = std::numeric_limits<int64_t>::max();

extern const char kVideoCodecH264[];
extern const char kAudioCodecAAC[];
extern const char kAudioCodecAC3[];
extern const char kAudioCodecEAC3[];

// A container for different types of media meta-data. Immutable.
class MediaFormat {
 public:
  MediaFormat(const MediaFormat& other);
  ~MediaFormat();

  bool operator==(const MediaFormat& other) const;
  bool operator!=(const MediaFormat& other) const { return !(*this == other); }

  static std::unique_ptr<MediaFormat> CreateVideoFormat(
      const std::string& track_id,
      const std::string& mime_type,
      const std::string& codecs,
      int32_t bitrate,
      int32_t max_input_size,
      int64_t duration_us,
      int32_t width,
      int32_t height,
      std::unique_ptr<char[]> initialization_data,
      size_t initialization_data_len,
      int32_t rotation_degrees = 0,
      double pixel_width_height_ratio = 1.0);

  static std::unique_ptr<MediaFormat> CreateAudioFormat(
      const std::string& track_id,
      const std::string& mime_type,
      const std::string& codecs,
      int32_t bitrate,
      int32_t max_input_size,
      int64_t duration_us,
      int32_t channel_count,
      int32_t sample_rate,
      std::unique_ptr<char[]> initialization_data,
      size_t initialization_data_len,
      const std::string& language = "en_US",
      int32_t pcm_encoding = kNoValue,
      DashChannelLayout channel_layout =
          DashChannelLayout::kChannelLayoutUnsupported,
      DashSampleFormat sample_format = DashSampleFormat::kSampleFormatUnknown);

  static std::unique_ptr<MediaFormat> CreateTextFormat(
      const std::string& track_id,
      const std::string& mime_type,
      int32_t bitrate,
      int64_t duration_us,
      const std::string& language = "en_US",
      int64_t subsample_offset_us = 0);

  static std::unique_ptr<MediaFormat> CreateImageFormat(
      const std::string& track_id,
      const std::string& mime_type,
      int32_t bitrate,
      int64_t duration_us,
      std::unique_ptr<char[]> initialization_data,
      size_t initialization_data_len,
      const std::string& language = "en_US");

  static std::unique_ptr<MediaFormat> CreateFormatForMimeType(
      const std::string& track_id,
      const std::string& mime_type,
      int32_t bitrate,
      int64_t duration_us);

  std::unique_ptr<MediaFormat> CopyWithSubsampleOffsetUs(
      int64_t subsample_offset_us) const;

  std::unique_ptr<MediaFormat> CopyAsAdaptive(
      const std::string& track_id) const;

  bool IsAdaptive() const { return adaptive_; }

  int32_t GetBitrate() const { return bitrate_; }

  int32_t GetChannelCount() const { return channel_count_; }

  int64_t GetDurationUs() const { return duration_us_; }

  int32_t GetEncoderDelay() const { return encoder_delay_; }

  int32_t GetEncoderPadding() const { return encoder_padding_; }

  int32_t GetHeight() const { return height_; }

  // The returned pointer is only valid for the lifetime of this class.
  const char* GetInitializationData() const {
    return initialization_data_.get();
  }

  size_t GetInitializationDataLen() const { return initialization_data_len_; }

  // The returned reference is only valid for the lifetime of this class.
  const std::string& GetLanguage() const { return language_; }

  int32_t GetMaxInputSize() const { return max_input_size_; }

  // The returned reference is only valid for the lifetime of this class.
  const std::string& GetMimeType() const { return mime_type_; }

  int32_t GetPcmEncoding() const { return pcm_encoding_; }

  double GetPixelWidthHeightRatio() const { return pixel_width_height_ratio_; }

  int32_t GetRotationDegrees() const { return rotation_degrees_; }

  int32_t GetSampleRate() const { return sample_rate_; }

  int64_t GetSubsampleOffsetUs() const { return subsample_offset_us_; }

  // The returned reference is only valid for the lifetime of this class.
  const std::string& GetTrackId() const { return track_id_; }

  int32_t GetWidth() const { return width_; }

  const std::string& GetCodecs() const { return codecs_; }

  const DashChannelLayout GetChannelLayout() const { return channel_layout_; }

  const DashSampleFormat GetSampleFormat() const { return sample_format_; }

  std::string DebugString() const;

  base::WeakPtr<const MediaFormat> AsWeakPtr() const;

 private:
  MediaFormat(const std::string& track_id,
              const std::string& mime_type,
              const std::string& codecs,
              int32_t bitrate,
              int32_t max_input_size,
              int64_t duration_us,
              int32_t width,
              int32_t height,
              int32_t rotation_degrees,
              double pixel_width_height_ratio,
              int32_t channel_count,
              int32_t sample_rate,
              const std::string& language,
              int64_t subsample_offset_us,
              std::unique_ptr<const char[]> initialization_data,
              size_t initialization_data_len,
              bool adaptive,
              int32_t pcm_encoding,
              int32_t encoder_delay,
              int32_t encoder_padding,
              DashChannelLayout channel_layout,
              DashSampleFormat sample_format);

  MediaFormat& operator=(const MediaFormat& other) = delete;

  static std::unique_ptr<char[]> CopyInitializationData(const MediaFormat& src);

  // The identifier for the track represented by the format, or null if unknown
  // or not applicable.
  const std::string track_id_;
  // The mime type of the format.
  const std::string mime_type_;
  // The average bandwidth in bits per second, or kNoValue if unknown or not
  // applicable.
  const int32_t bitrate_;
  // The maximum size of a buffer of data (typically one sample) in the format,
  // or kNoValue
  // if unknown or not applicable.
  const int32_t max_input_size_;
  // The duration in microseconds, or kUnknownTimeUs if the duration is unknown,
  // or
  // kMatchLongesUs if the duration should match the duration of the longest
  // track whose
  // duration is known.
  const int64_t duration_us_;
  // The width of the video in pixels, or kNoValue if unknown or not applicable.
  const int32_t width_;
  // The height of the video in pixels, or kNoValue if unknown or not
  // applicable.
  const int32_t height_;
  // The clockwise rotation that should be applied to the video for it to be
  // rendered in the correct
  // orientation, or kNoValue if unknown or not applicable. Only 0, 90, 180 and
  // 270 are
  // supported.
  const int32_t rotation_degrees_;
  // The width to height ratio of pixels in the video, or kNoValue if unknown or
  // not
  // applicable.
  const double pixel_width_height_ratio_;
  // The number of audio channels, or kNoValue if unknown or not applicable.
  const int32_t channel_count_;
  // The audio sampling rate in Hz, or kNoValue if unknown or not applicable.
  const int32_t sample_rate_;
  // For samples that contain subsamples, this is an offset that should be added
  // to subsample
  // timestamps. A value of kOffsetSampleRelative indicates that subsample
  // timestamps are
  // relative to the timestamps of their parent samples.
  const int64_t subsample_offset_us_;
  // Initialization data that must be provided to the decoder. May be
  // null if initialization data is not required.
  const std::unique_ptr<const char[]> initialization_data_;
  const size_t initialization_data_len_;
  // Whether the format represents an adaptive track, meaning that the format of
  // the actual media
  // data may change (e.g. to adapt to network conditions).
  const bool adaptive_;

  // The encoding for PCM audio streams. If mime_type is MimeTypes#AUDIO_RAW
  // then
  // one of ENCODING_PCM_8BIT, ENCODING_PCM_16BIT, ENCODING_PCM_24BIT
  // and ENCODING_PCM_32BIT. Set to kNoValue for other media types.
  const int32_t pcm_encoding_;
  // The number of samples to trim from the start of the decoded audio stream.
  const int32_t encoder_delay_;
  // The number of samples to trim from the end of the decoded audio stream.
  const int32_t encoder_padding_;

  DashChannelLayout channel_layout_;
  DashSampleFormat sample_format_;

  // The language of the track, or null if unknown or not applicable.
  const std::string language_;

  // The codecs of the track, or null if unknown or not applicable.
  const std::string codecs_;

  mutable base::WeakPtrFactory<const MediaFormat> weak_factory_;
};

}  // namespace ndash

#endif  // NDASH_MEDIA_FORMAT_H_
