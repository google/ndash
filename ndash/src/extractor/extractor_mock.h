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

#ifndef NDASH_EXTRACTOR_EXTRACTOR_MOCK_H_
#define NDASH_EXTRACTOR_EXTRACTOR_MOCK_H_

#include "extractor/extractor.h"
#include "gmock/gmock.h"

namespace ndash {
namespace extractor {

class MockExtractor : public ExtractorInterface {
 public:
  MockExtractor();
  ~MockExtractor() override;

  MOCK_METHOD1(Init, void(ExtractorOutputInterface*));
  MOCK_METHOD1(Sniff, bool(ExtractorInputInterface*));
  MOCK_METHOD2(Read, int(ExtractorInputInterface*, int64_t*));
  MOCK_METHOD0(Seek, void());
  MOCK_METHOD0(Release, void());
};

}  // namespace extractor
}  // namespace ndash

#endif  // NDASH_EXTRACTOR_EXTRACTOR_MOCK_H_
