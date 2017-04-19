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

#include "media_format.h"

#include <iomanip>
#include <sstream>
#include <string>

#include "base/logging.h"

namespace ndash {

const char kVideoCodecH264[] = "h264";
const char kAudioCodecAAC[] = "aac";
const char kAudioCodecAC3[] = "ac3";
const char kAudioCodecEAC3[] = "ec-3";

std::unique_ptr<MediaFormat> MediaFormat::CreateVideoFormat(
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
    int32_t rotation_degrees,
    double pixel_width_height_ratio) {
  return std::unique_ptr<MediaFormat>(new MediaFormat(
      track_id, mime_type, codecs, bitrate, max_input_size, duration_us, width,
      height, rotation_degrees, pixel_width_height_ratio, kNoValue, kNoValue,
      "", kOffsetSampleRelative, std::move(initialization_data),
      initialization_data_len, false, kNoValue, kNoValue, kNoValue,
      DashChannelLayout::kChannelLayoutUnsupported,
      DashSampleFormat::kSampleFormatUnknown));
}

std::unique_ptr<MediaFormat> MediaFormat::CreateAudioFormat(
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
    const std::string& language,
    int32_t pcm_encoding,
    DashChannelLayout channel_layout,
    DashSampleFormat sample_format) {
  return std::unique_ptr<MediaFormat>(new MediaFormat(
      track_id, mime_type, codecs, bitrate, max_input_size, duration_us,
      kNoValue, kNoValue, kNoValue, kNoValue, channel_count, sample_rate,
      language, kOffsetSampleRelative, std::move(initialization_data),
      initialization_data_len, false, pcm_encoding, kNoValue, kNoValue,
      channel_layout, sample_format));
}

std::unique_ptr<MediaFormat> MediaFormat::CreateTextFormat(
    const std::string& track_id,
    const std::string& mime_type,
    int32_t bitrate,
    int64_t duration_us,
    const std::string& language,
    int64_t subsample_offset_us) {
  return std::unique_ptr<MediaFormat>(new MediaFormat(
      track_id, mime_type, "", bitrate, kNoValue, duration_us, kNoValue,
      kNoValue, kNoValue, kNoValue, kNoValue, kNoValue, language,
      subsample_offset_us, nullptr, 0, false, kNoValue, kNoValue, kNoValue,
      DashChannelLayout::kChannelLayoutUnsupported,
      DashSampleFormat::kSampleFormatUnknown));
}

std::unique_ptr<MediaFormat> MediaFormat::CreateImageFormat(
    const std::string& track_id,
    const std::string& mime_type,
    int32_t bitrate,
    int64_t duration_us,
    std::unique_ptr<char[]> initialization_data,
    size_t initialization_data_len,
    const std::string& language) {
  return std::unique_ptr<MediaFormat>(new MediaFormat(
      track_id, mime_type, "", bitrate, kNoValue, duration_us, kNoValue,
      kNoValue, kNoValue, kNoValue, kNoValue, kNoValue, language,
      kOffsetSampleRelative, std::move(initialization_data),
      initialization_data_len, false, kNoValue, kNoValue, kNoValue,
      DashChannelLayout::kChannelLayoutUnsupported,
      DashSampleFormat::kSampleFormatUnknown));
}

std::unique_ptr<MediaFormat> MediaFormat::CreateFormatForMimeType(
    const std::string& track_id,
    const std::string& mime_type,
    int32_t bitrate,
    int64_t duration_us) {
  return std::unique_ptr<MediaFormat>(new MediaFormat(
      track_id, mime_type, "", bitrate, kNoValue, duration_us, kNoValue,
      kNoValue, kNoValue, kNoValue, kNoValue, kNoValue, "",
      kOffsetSampleRelative, nullptr, 0, false, kNoValue, kNoValue, kNoValue,
      DashChannelLayout::kChannelLayoutUnsupported,
      DashSampleFormat::kSampleFormatUnknown));
}

std::unique_ptr<MediaFormat> MediaFormat::CopyWithSubsampleOffsetUs(
    int64_t subsample_offset_us) const {
  std::unique_ptr<char[]> initialization_data_copy;
  if (initialization_data_) {
    initialization_data_copy.reset(new char[initialization_data_len_]);
    memcpy(initialization_data_copy.get(), initialization_data_.get(),
           initialization_data_len_);
  }

  return std::unique_ptr<MediaFormat>(new MediaFormat(
      track_id_, mime_type_, codecs_, bitrate_, max_input_size_, duration_us_,
      width_, height_, rotation_degrees_, pixel_width_height_ratio_,
      channel_count_, sample_rate_, language_, subsample_offset_us,
      std::move(initialization_data_copy), initialization_data_len_, adaptive_,
      pcm_encoding_, encoder_delay_, encoder_padding_,
      DashChannelLayout::kChannelLayoutUnsupported,
      DashSampleFormat::kSampleFormatUnknown));
}

std::unique_ptr<MediaFormat> MediaFormat::CopyAsAdaptive(
    const std::string& track_id) const {
  return std::unique_ptr<MediaFormat>(new MediaFormat(
      track_id, mime_type_, codecs_, kNoValue, kNoValue, duration_us_, kNoValue,
      kNoValue, kNoValue, kNoValue, kNoValue, kNoValue, "",
      kOffsetSampleRelative, nullptr, 0, true, kNoValue, kNoValue, kNoValue,
      DashChannelLayout::kChannelLayoutUnsupported,
      DashSampleFormat::kSampleFormatUnknown));
}

MediaFormat::MediaFormat(const std::string& track_id,
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
                         DashSampleFormat sample_format)
    : track_id_(track_id),
      mime_type_(mime_type),
      bitrate_(bitrate),
      max_input_size_(max_input_size),
      duration_us_(duration_us),
      width_(width),
      height_(height),
      rotation_degrees_(rotation_degrees),
      pixel_width_height_ratio_(pixel_width_height_ratio),
      channel_count_(channel_count),
      sample_rate_(sample_rate),
      subsample_offset_us_(subsample_offset_us),
      initialization_data_(std::move(initialization_data)),
      initialization_data_len_(initialization_data_len),
      adaptive_(adaptive),
      pcm_encoding_(pcm_encoding),
      encoder_delay_(encoder_delay),
      encoder_padding_(encoder_padding),
      channel_layout_(channel_layout),
      sample_format_(sample_format),
      language_(language),
      codecs_(codecs),
      weak_factory_(this) {
  DCHECK(!mime_type.empty());
}

MediaFormat::MediaFormat(const MediaFormat& other)
    : track_id_(other.track_id_),
      mime_type_(other.mime_type_),
      bitrate_(other.bitrate_),
      max_input_size_(other.max_input_size_),
      duration_us_(other.duration_us_),
      width_(other.width_),
      height_(other.height_),
      rotation_degrees_(other.rotation_degrees_),
      pixel_width_height_ratio_(other.pixel_width_height_ratio_),
      channel_count_(other.channel_count_),
      sample_rate_(other.sample_rate_),
      subsample_offset_us_(other.subsample_offset_us_),
      initialization_data_(CopyInitializationData(other)),
      initialization_data_len_(other.initialization_data_len_),
      adaptive_(other.adaptive_),
      pcm_encoding_(other.pcm_encoding_),
      encoder_delay_(other.encoder_delay_),
      encoder_padding_(other.encoder_padding_),
      channel_layout_(other.channel_layout_),
      sample_format_(other.sample_format_),
      language_(other.language_),
      codecs_(other.codecs_),
      weak_factory_(this) {}

MediaFormat::~MediaFormat() {}

bool MediaFormat::operator==(const MediaFormat& other) const {
  if (track_id_ != other.track_id_ || mime_type_ != other.mime_type_ ||
      codecs_ != other.codecs_ || bitrate_ != other.bitrate_ ||
      max_input_size_ != other.max_input_size_ ||
      duration_us_ != other.duration_us_ || width_ != other.width_ ||
      height_ != other.height_ ||
      rotation_degrees_ != other.rotation_degrees_ ||
      pixel_width_height_ratio_ != other.pixel_width_height_ratio_ ||
      channel_count_ != other.channel_count_ ||
      sample_rate_ != other.sample_rate_ ||
      subsample_offset_us_ != other.subsample_offset_us_ ||
      initialization_data_len_ != other.initialization_data_len_ ||
      adaptive_ != other.adaptive_ || pcm_encoding_ != other.pcm_encoding_ ||
      encoder_delay_ != other.encoder_delay_ ||
      encoder_padding_ != other.encoder_padding_ ||
      language_ != other.language_ ||
      channel_layout_ != other.channel_layout_ ||
      sample_format_ != other.sample_format_) {
    return false;
  }

  return (!initialization_data_ && !other.initialization_data_) ||
         memcmp(initialization_data_.get(), other.initialization_data_.get(),
                initialization_data_len_) == 0;
}

std::string MediaFormat::DebugString() const {
  std::ostringstream ss;
  ss << "MediaFormat[" << track_id_ << ", " << mime_type_ << ", " << codecs_
     << ", " << bitrate_ << ", " << max_input_size_ << ", " << duration_us_
     << ", " << width_ << ", " << height_ << ", " << rotation_degrees_ << ", "
     << pixel_width_height_ratio_ << ", " << channel_count_ << ", "
     << sample_rate_ << ", " << subsample_offset_us_ << channel_layout_ << ", "
     << sample_format_ << ", [" << initialization_data_len_ << "]{ ";

  if (initialization_data_) {
    for (size_t i = 0; i < initialization_data_len_; i++) {
      ss << std::setw(2) << std::hex << int(initialization_data_[i]) << " ";
    }
  } else {
    ss << "NULL ";
  }

  ss << "}, " << adaptive_ << ", " << pcm_encoding_ << ", " << encoder_delay_
     << ", " << encoder_padding_ << ", " << language_ << "]";
  return ss.str();
}

base::WeakPtr<const MediaFormat> MediaFormat::AsWeakPtr() const {
  return weak_factory_.GetWeakPtr();
}

std::unique_ptr<char[]> MediaFormat::CopyInitializationData(
    const MediaFormat& src) {
  std::unique_ptr<char[]> initialization_data;

  if (src.initialization_data_) {
    initialization_data.reset(new char[src.initialization_data_len_]);
    memcpy(initialization_data.get(), src.initialization_data_.get(),
           src.initialization_data_len_);
  }

  return initialization_data;
}

}  // namespace ndash
