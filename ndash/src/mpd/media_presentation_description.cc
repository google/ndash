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

#include "mpd/media_presentation_description.h"

namespace ndash {

namespace mpd {

MediaPresentationDescription::MediaPresentationDescription(
    int64_t availability_start_time,
    int64_t duration,
    int64_t min_buffer_time,
    bool dynamic,
    int64_t min_update_period,
    int64_t time_shift_buffer_depth,
    std::unique_ptr<DescriptorType> utc_timing,
    std::string location,
    std::vector<std::unique_ptr<Period>>* periods,
    std::vector<std::unique_ptr<DescriptorType>>* supplemental_properties,
    std::vector<std::unique_ptr<DescriptorType>>* essential_properties)
    : availability_start_time_(availability_start_time),
      duration_(duration),
      min_buffer_time_(min_buffer_time),
      dynamic_(dynamic),
      min_update_period_(min_update_period),
      time_shift_buffer_depth_(time_shift_buffer_depth),
      utc_timing_(std::move(utc_timing)),
      location_(location) {
  if (periods != nullptr) {
    periods_ = std::move(*periods);
  }
  if (supplemental_properties != nullptr) {
    supplemental_properties_ = std::move(*supplemental_properties);
  }
  if (essential_properties != nullptr) {
    essential_properties_ = std::move(*essential_properties);
  }
}

MediaPresentationDescription::~MediaPresentationDescription() {}

const std::string& MediaPresentationDescription::GetNextManifestUri() const {
  return location_;
}

size_t MediaPresentationDescription::GetPeriodCount() const {
  return periods_.size();
}

Period* MediaPresentationDescription::GetPeriod(int index) {
  return periods_.at(index).get();
}

const Period* MediaPresentationDescription::GetPeriod(int index) const {
  return periods_.at(index).get();
}

int64_t MediaPresentationDescription::GetPeriodDuration(int32_t index) const {
  if (index < 0 || index >= periods_.size()) {
    return -1;
  }
  if (index == periods_.size() - 1) {
    if (duration_ == -1) {
      return -1;
    } else {
      int64_t period_end = periods_.at(0)->GetStartMs() + duration_;
      return period_end - periods_.at(index)->GetStartMs();
    }
  } else {
    return periods_.at(index + 1)->GetStartMs() -
           periods_.at(index)->GetStartMs();
  }
}

size_t MediaPresentationDescription::GetSupplementalPropertyCount() const {
  return supplemental_properties_.size();
}

const DescriptorType* MediaPresentationDescription::GetSupplementalProperty(
    int index) const {
  return supplemental_properties_.at(index).get();
}

size_t MediaPresentationDescription::GetEssentialPropertyCount() const {
  return essential_properties_.size();
}

const DescriptorType* MediaPresentationDescription::GetEssentialProperty(
    int index) const {
  return essential_properties_.at(index).get();
}

}  // namespace mpd

}  // namespace ndash
