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

#ifndef NDASH_EXTRACTOR_EXTRACTOR_OUTPUT_MOCK_H_
#define NDASH_EXTRACTOR_EXTRACTOR_OUTPUT_MOCK_H_

#include "drm/drm_init_data.h"
#include "extractor/extractor_output.h"
#include "gmock/gmock.h"

namespace ndash {

namespace drm {
class DrmInitDataInterface;
}  // namespace drm

namespace extractor {

class SeekMapInterface;

class MockExtractorOutput : public ExtractorOutputInterface {
 public:
  MockExtractorOutput();
  ~MockExtractorOutput() override;

  MOCK_METHOD1(RegisterTrack, TrackOutputInterface*(int));
  MOCK_METHOD0(DoneRegisteringTracks, void());
  MOCK_METHOD1(SetDrmInitData,
               void(scoped_refptr<const drm::RefCountedDrmInitData>));

  void GiveSeekMap(std::unique_ptr<const SeekMapInterface> seek_map) override;
  MOCK_METHOD1(GiveSeekMapMock, void(const SeekMapInterface*));

  std::unique_ptr<const SeekMapInterface> given_seek_map_;
};

}  // namespace extractor
}  // namespace ndash

#endif  // NDASH_EXTRACTOR_EXTRACTOR_OUTPUT_MOCK_H_
