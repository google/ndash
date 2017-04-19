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

#ifndef NDASH_UTIL_SLIDING_MEDIAN_H_
#define NDASH_UTIL_SLIDING_MEDIAN_H_

#include <cstdint>
#include <map>
#include <queue>
#include <utility>

#include "util/averager.h"

namespace ndash {
namespace util {

// Calculate a median over a sliding window of weighted values. A maximum total
// weight is configured. Once the maximum weight is reached, the oldest value
// is reduced in weight until it reaches zero and is removed. This maintains a
// constant total weight at steady state. The values are stored in a form
// similar in spirit to run-length encoding.
//
// This can be trivially extended to calculate any percentile.
//
// SlidingMedian can be used for bandwidth estimation based on a sliding window
// of past download rate observations. This is an alternative to sliding mean
// and exponential averaging which suffer from susceptibility to outliers and
// slow adaptation to step functions.
//
//
// GetMedian
//
// See http://en.wikipedia.org/wiki/Moving_average and
// http://en.wikipedia.org/wiki/Selection_algorithm
class SlidingMedian : public AveragerInterface {
 public:
  // max_weight must be positive
  explicit SlidingMedian(SampleWeight max_weight);
  ~SlidingMedian();

  // Record a new observation. Respect the configured total weight by reducing
  // in weight or removing the oldest observations as required.
  //
  // weight: The weight of the new observation (must be positive)
  // value: The value of the new observation.
  //
  // Complexity: O(log max_weight) normally, but worst case O(max_weight) if
  // the new sample's weight is large compared to the other samples (it may
  // wipe out the entire set of previous samples, and the previous samples may
  // all have weight=1).
  void AddSample(SampleWeight weight, SampleValue value) override;

  // Returns true iff GetAverage() will return a valid result.
  bool HasSample() const override;

  // Compute the median by integration.
  //
  // Returns the median value or 0 (if there are no samples).
  //
  // Complexity: O(max_weight) because there is a linear scan to find the first
  //             sample with a large enough cumulative weight. There are
  //             probably faster algorithms to find the median value in an
  //             array (see std::nth_element), but there was no obviously
  //             better approach that allows for weighted samples.
  SampleValue GetAverage() const;

 private:
  typedef std::multimap<SampleValue, SampleWeight> SampleMap;
  typedef std::queue<SampleMap::iterator> SampleQueue;

  const SampleWeight max_weight_;

  SampleQueue samples_by_index_;
  SampleMap samples_by_value_;

  SampleWeight total_weight_ = 0;
};

}  // namespace util
}  // namespace ndash

#endif  // NDASH_UTIL_SLIDING_MEDIAN_H_
