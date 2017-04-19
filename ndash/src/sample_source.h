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

#ifndef NDASH_SAMPLE_SOURCE_H_
#define NDASH_SAMPLE_SOURCE_H_

#include <cstdint>

#include "sample_source_reader.h"

namespace ndash {

// A source of media samples.
//
// A SampleSource may expose one or multiple tracks. The number of tracks and
// each track's media format can be queried using
// SampleSourceReader::GetTrackCount() and SampleSourceReader::GtFormat(int)
// respectively.
class SampleSourceInterface {
 public:
  SampleSourceInterface(){};
  virtual ~SampleSourceInterface(){};
  // A consumer of samples should call this method to register themselves and
  // gain access to the source through the returned SampleSourceReaderInterface.
  //
  // SampleSourceReader::Release() should be called on the returned object when
  // access is no longer required.
  virtual SampleSourceReaderInterface* Register() = 0;
};

}  // namespace ndash

#endif  // NDASH_SAMPLE_SOURCE_H_
