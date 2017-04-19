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

#ifndef NDASH_MEDIA_SAMPLE_SOURCE_READER_MOCK_H_
#define NDASH_MEDIA_SAMPLE_SOURCE_READER_MOCK_H_

#include "gmock/gmock.h"
#include "sample_source_reader.h"

namespace ndash {

class SampleSourceReaderMock : public SampleSourceReaderInterface {
 public:
  SampleSourceReaderMock();
  ~SampleSourceReaderMock() override;

  MOCK_METHOD0(CanContinueBuffering, bool());
  MOCK_METHOD1(Prepare, bool(int64_t));
  MOCK_METHOD0(GetDurationUs, int64_t());
  MOCK_METHOD2(Enable, void(const TrackCriteria*, int64_t));
  MOCK_METHOD1(ContinueBuffering, bool(int64_t));
  MOCK_METHOD0(ReadDiscontinuity, int64_t());
  MOCK_METHOD3(ReadData,
               SampleSourceReaderInterface::ReadResult(int64_t,
                                                       MediaFormatHolder*,
                                                       SampleHolder*));
  MOCK_METHOD1(SeekToUs, void(int64_t));
  MOCK_METHOD0(GetBufferedPositionUs, int64_t());
  MOCK_METHOD1(Disable, void(const base::Closure*));
  MOCK_METHOD0(Release, void());
};

}  // namespace ndash

#endif  // NDASH_MEDIA_SAMPLE_SOURCE_READER_MOCK_H_
