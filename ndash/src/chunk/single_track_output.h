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

#ifndef NDASH_CHUNK_SINGLE_TRACK_OUTPUT_H_
#define NDASH_CHUNK_SINGLE_TRACK_OUTPUT_H_

#include "base/memory/ref_counted.h"
#include "extractor/track_output.h"

namespace ndash {

namespace drm {
class RefCountedDrmInitData;
}  // namespace drm

namespace extractor {
class SeekMapInterface;
}  // namespace extractor

namespace chunk {

// Receives stream level data extracted by the wrapped Extractor. Intended to
// be used in conjunction with ChunkExtractorWrapper.
class SingleTrackOutputInterface : public extractor::TrackOutputInterface {
 public:
  SingleTrackOutputInterface() {}
  ~SingleTrackOutputInterface() override {}

  // See ExtractorOutput::GiveSeekMap()
  virtual void GiveSeekMap(
      std::unique_ptr<const extractor::SeekMapInterface> seek_map) = 0;

  // See ExtractorOutput::SetDrmInitData()
  virtual void SetDrmInitData(
      scoped_refptr<const drm::RefCountedDrmInitData> drm_init_data) = 0;
};

}  // namespace chunk
}  // namespace ndash

#endif  // NDASH_CHUNK_SINGLE_TRACK_OUTPUT_H_
