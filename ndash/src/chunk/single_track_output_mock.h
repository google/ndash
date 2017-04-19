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

#ifndef NDASH_CHUNK_SINGLE_TRACK_OUTPUT_MOCK_H_
#define NDASH_CHUNK_SINGLE_TRACK_OUTPUT_MOCK_H_

#include <cstdint>
#include <cstdlib>

#include "chunk/single_track_output.h"
#include "drm/drm_init_data.h"
#include "gmock/gmock.h"

namespace ndash {

namespace dash {
class MediaFormat;
}  // namespace dash

namespace drm {
class DrmInitDataInterface;
}  // namespace drm

namespace extractor {
class SeekMapInterface;
}  // namespace extractor

namespace chunk {

class MockSingleTrackOutput : public SingleTrackOutputInterface {
 public:
  MockSingleTrackOutput();
  ~MockSingleTrackOutput() override;

  // SingleTrackOutputInterface
  void GiveSeekMap(
      std::unique_ptr<const extractor::SeekMapInterface> seek_map) override;
  MOCK_METHOD1(GiveSeekMapMock, void(const extractor::SeekMapInterface*));
  MOCK_METHOD1(SetDrmInitData,
               void(scoped_refptr<const drm::RefCountedDrmInitData>));

  // TrackOutputInterface
  MOCK_METHOD4(
      WriteSampleData,
      bool(extractor::ExtractorInputInterface*, size_t, bool, int64_t*));
  MOCK_METHOD2(WriteSampleData, void(const void*, size_t));
  MOCK_METHOD4(WriteSampleDataFixThis,
               bool(const void*, size_t, bool, int64_t*));
  MOCK_METHOD9(WriteSampleMetadata,
               void(int64_t,
                    int64_t,
                    int32_t,
                    size_t,
                    size_t,
                    const std::string*,
                    const std::string*,
                    std::vector<int32_t>*,
                    std::vector<int32_t>*));

  void GiveFormat(std::unique_ptr<const MediaFormat> format) override;
  MOCK_METHOD1(GiveFormatMock, void(const MediaFormat*));

  std::unique_ptr<const MediaFormat> given_format_;
  std::unique_ptr<const extractor::SeekMapInterface> given_seek_map_;
};

}  // namespace chunk
}  // namespace ndash

#endif  // NDASH_CHUNK_SINGLE_TRACK_OUTPUT_MOCK_H_
