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

#ifndef NDASH_EXTRACTOR_EXTRACTOR_INPUT_MOCK_H_
#define NDASH_EXTRACTOR_EXTRACTOR_INPUT_MOCK_H_

#include <cstdint>
#include <cstdlib>
#include "extractor/extractor_input.h"
#include "gmock/gmock.h"

namespace ndash {
namespace extractor {

class MockExtractorInput : public ExtractorInputInterface {
 public:
  MockExtractorInput();
  ~MockExtractorInput() override;

  MOCK_METHOD2(Read, ssize_t(void*, size_t));
  MOCK_METHOD3(ReadFully, bool(void*, size_t, bool*));
  MOCK_METHOD1(Skip, ssize_t(size_t length));
  MOCK_METHOD2(SkipFully, bool(size_t length, bool* end_of_input));
  MOCK_METHOD3(PeekFully,
               bool(void* target, size_t length, bool* end_of_input));
  MOCK_METHOD2(AdvancePeekPosition, bool(size_t length, bool* end_of_input));
  MOCK_METHOD0(ResetPeekPosition, void());
  MOCK_CONST_METHOD0(GetPeekPosition, int64_t());
  MOCK_CONST_METHOD0(GetPosition, int64_t());
  MOCK_CONST_METHOD0(GetLength, int64_t());
};

}  // namespace extractor
}  // namespace ndash

#endif  // NDASH_EXTRACTOR_EXTRACTOR_INPUT_MOCK_H_
