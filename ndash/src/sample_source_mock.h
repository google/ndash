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

#ifndef NDASH_MEDIA_SAMPLE_SOURCE_MOCK_H_
#define NDASH_MEDIA_SAMPLE_SOURCE_MOCK_H_

#include "gmock/gmock.h"
#include "sample_source.h"

namespace ndash {

class SampleSourceMock : public SampleSourceInterface {
 public:
  SampleSourceMock();
  ~SampleSourceMock() override;

  MOCK_METHOD0(Register, SampleSourceReaderInterface*(void));
};

}  // namespace ndash

#endif  // NDASH_MEDIA_SAMPLE_SOURCE_MOCK_H_
