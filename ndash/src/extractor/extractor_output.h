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

#ifndef NDASH_EXTRACTOR_EXTRACTOR_OUTPUT_H_
#define NDASH_EXTRACTOR_EXTRACTOR_OUTPUT_H_

#include <memory>
#include "base/memory/ref_counted.h"

namespace ndash {

namespace drm {
class RefCountedDrmInitData;
}  // namespace drm

namespace extractor {

class SeekMapInterface;
class TrackOutputInterface;

// Receives stream level data extracted by an Extractor
class ExtractorOutputInterface {
 public:
  ExtractorOutputInterface() {}
  virtual ~ExtractorOutputInterface() {}

  // Invoked when the Extractor identifies the existence of a track in the
  // stream.
  // Returns a TrackOutputInterface that will receive track level data
  // belonging to the track. This object retains ownership of the returned
  // TrackOutputInterface.
  // track_id: A track identifier.
  //
  // After DoneRegisteringTracks() has been called, returns nullptr and does
  // nothing.
  virtual TrackOutputInterface* RegisterTrack(int track_id) = 0;

  // Invoked when all tracks have been identified, meaning that RegisterTrack()
  // will not be invoked again.
  virtual void DoneRegisteringTracks() = 0;

  // Invoked when a SeekMap has been extracted from the stream.
  // seek_map: The extracted SeekMap.
  virtual void GiveSeekMap(
      std::unique_ptr<const SeekMapInterface> seek_map) = 0;

  // Invoked when DrmInitData has been extracted from the stream.
  // drm_init_data: The extracted DrmInitData.
  virtual void SetDrmInitData(
      scoped_refptr<const drm::RefCountedDrmInitData> drm_init_data) = 0;
};

}  // namespace extractor
}  // namespace ndash

#endif  // NDASH_EXTRACTOR_EXTRACTOR_OUTPUT_H_
