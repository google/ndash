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

#ifndef NDASH_DASH_PERIOD_HOLDER_H_
#define NDASH_DASH_PERIOD_HOLDER_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "drm/drm_init_data.h"
#include "util/const_unique_ptr_map_value_iterator.h"

namespace ndash {

class TrackCriteria;

namespace drm {
class DrmInitDataInterface;
class DrmSessionManagerInterface;
}  // namespace drm

namespace mpd {
class AdaptationSet;
class DashSegmentIndexInterface;
class MediaPresentationDescription;
class Period;
class Representation;
}  // namespace mpd

namespace dash {

class RepresentationHolder;

// Internal to DashChunkSource, holds periods with some extra metadata
class PeriodHolder {
 public:
  typedef std::map<std::string, std::unique_ptr<RepresentationHolder>>
      RepresentationHolderMap;
  typedef util::ConstUniquePtrMapValueRange<RepresentationHolderMap>
      RepresentationHolderConstRange;

  PeriodHolder(drm::DrmSessionManagerInterface* drm_session_manager,
               int32_t local_index,
               const mpd::MediaPresentationDescription& manifest,
               int32_t manifest_index,
               const TrackCriteria* track_criteria,
               float playback_rate = 1);
  ~PeriodHolder();

  int32_t local_index() const { return local_index_; }
  base::TimeDelta start_time() const { return start_time_; }
  const std::map<std::string, std::unique_ptr<RepresentationHolder>>&
  representation_holders() {
    return representation_holders_;
  }
  size_t num_representation_holders() const {
    return representation_holders_.size();
  }
  RepresentationHolder* GetRepresentationHolder(const std::string& id) {
    auto find_result = representation_holders_.find(id);
    if (find_result != representation_holders_.end()) {
      return find_result->second.get();
    } else {
      return nullptr;
    }
  }
  const RepresentationHolder* GetRepresentationHolder(
      const std::string& id) const {
    auto find_result = representation_holders_.find(id);
    if (find_result != representation_holders_.end()) {
      return find_result->second.get();
    } else {
      return nullptr;
    }
  }
  RepresentationHolderConstRange RepresentationHolderValues() const {
    return RepresentationHolderConstRange(&representation_holders_);
  }

  const std::vector<int32_t>& representation_indices() const {
    return representation_indices_;
  }

  base::TimeDelta available_start_time() const { return available_start_time_; }

  const base::TimeDelta* GetAvailableEndTime() const {
    if (index_is_unbounded_) {
      return nullptr;
    }
    return &available_end_time_;
  }

  bool index_is_unbounded() const { return index_is_unbounded_; }
  bool index_is_explicit() const { return index_is_explicit_; }
  scoped_refptr<const drm::RefCountedDrmInitData> drm_init_data() const {
    return drm_init_data_.get();
  }

  void SetDrmInitData(
      scoped_refptr<const drm::RefCountedDrmInitData> drm_init_data) {
    drm_init_data_ = std::move(drm_init_data);
  }

  bool UpdatePeriod(const mpd::MediaPresentationDescription& manifest,
                    int32_t manifest_index,
                    const TrackCriteria* track_criteria);

  // For when any representation will do. This pointer is only valid for the
  // current DashThread task.  It is not safe to store it since DashThread
  // update may cause it to become invalid before the next tasks's execution.
  const mpd::DashSegmentIndexInterface* GetArbitrarySegmentIndex() const;

 private:
  // Initialized by constructor
  const int32_t local_index_;
  const base::TimeDelta start_time_;

  // Filled in by constructor
  std::map<std::string, std::unique_ptr<RepresentationHolder>>
      representation_holders_;
  std::vector<int32_t> representation_indices_;
  scoped_refptr<const drm::RefCountedDrmInitData> drm_init_data_;
  drm::DrmSessionManagerInterface* drm_session_manager_;

  // Initialized by UpdateRepresentationIndependentProperties()
  bool index_is_unbounded_ = true;
  bool index_is_explicit_ = false;
  base::TimeDelta available_start_time_;
  base::TimeDelta available_end_time_;

  PeriodHolder(const PeriodHolder& other) = delete;
  PeriodHolder& operator=(const PeriodHolder& other) = delete;

  void UpdateRepresentationIndependentProperties(
      base::TimeDelta period_duration,
      const mpd::DashSegmentIndexInterface* segment_index);

  // Apply track_criteria to the adapatation sets found in |period| and
  // select one.
  static const mpd::AdaptationSet* SelectAdaptationSet(
      const mpd::Period& period,
      const TrackCriteria* track_criteria);

  static bool GetRepresentationIndex(const mpd::AdaptationSet& adaptation_set,
                                     const std::string& format_id,
                                     int32_t* found_index);

  static scoped_refptr<const drm::RefCountedDrmInitData> BuildDrmInitData(
      const mpd::AdaptationSet& adaptation_set);
  static base::TimeDelta GetPeriodDuration(
      const mpd::MediaPresentationDescription& manifest,
      int32_t index);
};

}  // namespace dash
}  // namespace ndash

#endif  // NDASH_DASH_PERIOD_HOLDER_H_
