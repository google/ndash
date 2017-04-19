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

#ifndef NDASH_DASH_DASH_TRACK_SELECTOR_OUTPUT_MOCK_H_
#define NDASH_DASH_DASH_TRACK_SELECTOR_OUTPUT_MOCK_H_

#include <cstdint>
#include <vector>

#include "dash/dash_track_selector_output.h"
#include "gmock/gmock.h"

namespace ndash {

namespace mpd {
class MediaPresentationDescription;
}  // namespace mpd

namespace dash {

class MockDashTrackSelectorOutput : public DashTrackSelectorOutputInterface {
 public:
  MockDashTrackSelectorOutput();
  ~MockDashTrackSelectorOutput() override;

  MOCK_METHOD4(AdaptiveTrack,
               void(const mpd::MediaPresentationDescription* manifest,
                    int32_t period_index,
                    int32_t adaptation_set_index,
                    const std::vector<int32_t>* representation_indices));
  MOCK_METHOD4(FixedTrack,
               void(const mpd::MediaPresentationDescription* manifest,
                    int32_t period_index,
                    int32_t adaptation_set_index,
                    int32_t representation_index));
};

}  // namespace dash
}  // namespace ndash

#endif  // NDASH_DASH_DASH_TRACK_SELECTOR_OUTPUT_MOCK_H_
