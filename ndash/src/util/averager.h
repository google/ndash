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

#ifndef NDASH_UTIL_AVERAGER_H_
#define NDASH_UTIL_AVERAGER_H_

#include <cstdint>

namespace ndash {
namespace util {

// Computes some kind of moving average
class AveragerInterface {
 public:
  typedef int64_t SampleValue;
  typedef int32_t SampleWeight;

  virtual ~AveragerInterface() {}

  // Record a new observation, with an associated weight. The exact meaning of
  // the weight is dependent on implementation.
  //
  // weight: The weight of the new observation (must be positive)
  // value: The value of the new observation.
  //
  // Complexity is dependent on implementation.
  virtual void AddSample(SampleWeight weight, SampleValue value) = 0;

  // Returns true iff GetAverage() will return a valid result.
  virtual bool HasSample() const = 0;

  // Compute the average. What kind of average depends on implementation.
  //
  // Returns the average or 0 (if there are no samples).
  //
  // Complexity is dependent on implementation.
  virtual SampleValue GetAverage() const = 0;

 protected:
  AveragerInterface() {}

 private:
  AveragerInterface(const AveragerInterface& other) = delete;
  AveragerInterface& operator=(const AveragerInterface& other) = delete;
};

}  // namespace util
}  // namespace ndash

#endif  // NDASH_UTIL_AVERAGER_H_
