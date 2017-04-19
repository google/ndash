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

#ifndef NDASH_DASH_DASH_TRACK_SELECTOR_OUTPUT_H_
#define NDASH_DASH_DASH_TRACK_SELECTOR_OUTPUT_H_

#include <cstdint>
#include <vector>

namespace ndash {

namespace mpd {
class MediaPresentationDescription;
}  // namespace mpd

namespace dash {

class DashTrackSelectorOutputInterface {
 public:
  DashTrackSelectorOutputInterface() {}
  virtual ~DashTrackSelectorOutputInterface() {}

  // Outputs an adaptive track, covering the specified representations in the
  // specified adaptation set.
  //
  // manifest: The media presentation description being processed.
  // period_index: The index of the period being processed.
  // adaptation_set_index: The index of the adaptation set within which the
  //                       representations are located.
  // representation_indices: The indices of the track within the element.
  virtual void AdaptiveTrack(
      const mpd::MediaPresentationDescription* manifest,
      int32_t period_index,
      int32_t adaptation_set_index,
      const std::vector<int32_t>* representation_indices) = 0;

  // Outputs an fixed track corresponding to the specified representation in
  // the specified adaptation set.
  //
  // manifest: The media presentation description being processed.
  // period_index: The index of the period being processed.
  // adaptation_set_index: The index of the adaptation set within which the
  //                       track is located.
  // representation_index: The index of the representation within the
  //                       adaptation set.
  virtual void FixedTrack(const mpd::MediaPresentationDescription* manifest,
                          int32_t period_index,
                          int32_t adaptation_set_index,
                          int32_t representation_index) = 0;
};

}  // namespace dash
}  // namespace ndash

#endif  // NDASH_DASH_DASH_TRACK_SELECTOR_OUTPUT_H_
