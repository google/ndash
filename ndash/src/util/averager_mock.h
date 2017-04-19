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

#ifndef NDASH_UTIL_AVERAGER_MOCK_H_
#define NDASH_UTIL_AVERAGER_MOCK_H_

#include "gmock/gmock.h"
#include "util/averager.h"

namespace ndash {
namespace util {

class MockAverager : public AveragerInterface {
 public:
  MockAverager();
  ~MockAverager() override;

  MOCK_METHOD2(AddSample, void(SampleWeight weight, SampleValue value));
  MOCK_CONST_METHOD0(HasSample, bool());
  MOCK_CONST_METHOD0(GetAverage, SampleValue());

 protected:
  MockAverager(const MockAverager& other) = delete;
  MockAverager& operator=(const MockAverager& other) = delete;
};

}  // namespace util
}  // namespace ndash

#endif  // NDASH_UTIL_AVERAGER_MOCK_H_
