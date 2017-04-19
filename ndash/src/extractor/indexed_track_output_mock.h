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

#ifndef NDASH_EXTRACTOR_INDEXED_TRACK_OUTPUT_MOCK_H_
#define NDASH_EXTRACTOR_INDEXED_TRACK_OUTPUT_MOCK_H_

#include <cstdint>

#include "extractor/indexed_track_output.h"
#include "gmock/gmock.h"

namespace ndash {

class MediaFormat;

namespace extractor {

class ExtractorInputInterface;

class MockIndexedTrackOutput : public IndexedTrackOutputInterface {
 public:
  MockIndexedTrackOutput();
  ~MockIndexedTrackOutput() override;

  MOCK_METHOD4(WriteSampleData,
               bool(ExtractorInputInterface*, size_t, bool, int64_t*));
  MOCK_METHOD2(WriteSampleData, void(const void*, size_t));
  MOCK_METHOD4(WriteSampleDataFixThis,
               bool(const void*, size_t, bool, int64_t*));
  MOCK_METHOD9(WriteSampleMetadata,
               void(int64_t,
                    int64_t,
                    int32_t,
                    size_t,
                    size_t,
                    const std::string* encryption_key_id,
                    const std::string* iv,
                    std::vector<int32_t>*,
                    std::vector<int32_t>*));
  MOCK_CONST_METHOD0(GetWriteIndex, int32_t());

  void GiveFormat(std::unique_ptr<const MediaFormat> format) override;
  MOCK_METHOD1(GiveFormatMock, void(const MediaFormat*));

  std::unique_ptr<const MediaFormat> given_format_;
};

}  // namespace extractor
}  // namespace ndash

#endif  // NDASH_EXTRACTOR_INDEXED_TRACK_OUTPUT_H_
