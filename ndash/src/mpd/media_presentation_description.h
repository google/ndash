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

#ifndef NDASH_MPD_MEDIA_PRESENTATION_DESCRIPTION_H_
#define NDASH_MPD_MEDIA_PRESENTATION_DESCRIPTION_H_

#include <cstdint>
#include <memory>

#include "base/memory/ref_counted.h"
#include "mpd/descriptor_type.h"
#include "mpd/period.h"

namespace ndash {

namespace mpd {

// Represents a DASH media presentation description (mpd).
class MediaPresentationDescription
    : public base::RefCounted<MediaPresentationDescription> {
 public:
  MediaPresentationDescription(
      int64_t availability_start_time,
      int64_t duration,
      int64_t min_buffer_time,
      bool dynamic,
      int64_t min_update_period,
      int64_t time_shift_buffer_depth,
      std::unique_ptr<DescriptorType> utc_timing,
      std::string location,
      std::vector<std::unique_ptr<Period>>* periods,
      std::vector<std::unique_ptr<DescriptorType>>* supplemental_properties =
          nullptr,
      std::vector<std::unique_ptr<DescriptorType>>* essential_properties =
          nullptr);

  // Returns the next manifest uri. The reference is only valid for the
  // lifetime of this class.
  const std::string& GetNextManifestUri() const;
  size_t GetPeriodCount() const;
  Period* GetPeriod(int32_t index);
  const Period* GetPeriod(int32_t index) const;
  int64_t GetPeriodDuration(int32_t index) const;
  int64_t GetAvailabilityStartTime() const { return availability_start_time_; }
  int64_t GetDuration() const { return duration_; }
  int64_t GetMinBufferTime() const { return min_buffer_time_; }
  bool IsDynamic() const { return dynamic_; }
  int64_t GetMinUpdatePeriod() const { return min_update_period_; }
  int64_t GetTimeShiftBufferDepth() const { return time_shift_buffer_depth_; }
  DescriptorType* GetUtcTiming() const { return utc_timing_.get(); }
  size_t GetSupplementalPropertyCount() const;
  const DescriptorType* GetSupplementalProperty(int32_t index) const;
  size_t GetEssentialPropertyCount() const;
  const DescriptorType* GetEssentialProperty(int32_t index) const;

 private:
  friend class base::RefCounted<MediaPresentationDescription>;

  ~MediaPresentationDescription();

  int64_t availability_start_time_;
  int64_t duration_;
  int64_t min_buffer_time_;
  bool dynamic_;
  int64_t min_update_period_;
  int64_t time_shift_buffer_depth_;
  std::unique_ptr<DescriptorType> utc_timing_;
  std::string location_;
  std::vector<std::unique_ptr<Period>> periods_;
  std::vector<std::unique_ptr<DescriptorType>> supplemental_properties_;
  std::vector<std::unique_ptr<DescriptorType>> essential_properties_;
};

}  // namespace mpd

}  // namespace ndash

#endif  // NDASH_MPD_MEDIA_PRESENTATION_DESCRIPTION_H_
