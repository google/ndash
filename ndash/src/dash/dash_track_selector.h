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

#ifndef NDASH_DASH_DASH_TRACK_SELECTOR_H_
#define NDASH_DASH_DASH_TRACK_SELECTOR_H_

#include <cstdint>

namespace ndash {

namespace mpd {
class MediaPresentationDescription;
}  // namespace mpd

namespace dash {

class DashTrackSelectorOutputInterface;

class DashTrackSelectorInterface {
 public:
  DashTrackSelectorInterface() {}
  virtual ~DashTrackSelectorInterface() {}

  // Outputs a track selection for a given period.
  //
  // manifest: the media presentation description to process.
  // period_index: The index of the period to process.
  // output: The output to receive tracks.
  //
  // Returns true if the period was processed OK, false if an error occurs.
  virtual bool SelectTracks(const mpd::MediaPresentationDescription* manifest,
                            int32_t period_index,
                            DashTrackSelectorOutputInterface* output) = 0;
};

}  // namespace dash
}  // namespace ndash

#endif  // NDASH_DASH_DASH_TRACK_SELECTOR_H_
