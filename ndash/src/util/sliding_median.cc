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

#include "util/sliding_median.h"

#include <map>
#include <queue>
#include <utility>

#include "base/logging.h"

namespace ndash {
namespace util {

SlidingMedian::SlidingMedian(SampleWeight max_weight)
    : max_weight_(max_weight) {
  CHECK_GT(max_weight, 0);
}

SlidingMedian::~SlidingMedian() {}

void SlidingMedian::AddSample(SampleWeight weight, SampleValue value) {
  CHECK_GT(weight, 0);

  VLOG(1) << "New sample weight=" << weight << "; value=" << value
          << "; previous total weight=" << total_weight_;

  SampleMap::iterator new_sample =
      samples_by_value_.insert(std::make_pair(value, weight));
  samples_by_index_.push(new_sample);
  total_weight_ += weight;

  int elements_dropped = 0;
  while (total_weight_ > max_weight_) {
    SampleWeight excess_weight = total_weight_ - max_weight_;
    SampleMap::iterator oldest_sample = samples_by_index_.front();
    if (oldest_sample->second <= excess_weight) {
      total_weight_ -= oldest_sample->second;
      samples_by_value_.erase(oldest_sample);
      samples_by_index_.pop();
      elements_dropped++;
    } else {
      oldest_sample->second -= excess_weight;
      total_weight_ -= excess_weight;
      break;
    }
  }

  VLOG(2) << "Dropped " << elements_dropped << " elements to fit weight";

  DCHECK_GE(total_weight_, 0);
}

bool SlidingMedian::HasSample() const {
  return !samples_by_value_.empty();
}

SlidingMedian::SampleValue SlidingMedian::GetAverage() const {
  SampleWeight desired_weight = (total_weight_ / 2) + (total_weight_ % 2);

  SampleWeight accumulated_weight = 0;
  int index = 0;
  for (const auto& sample : samples_by_value_) {
    accumulated_weight += sample.second;
    if (accumulated_weight >= desired_weight) {
      VLOG(2) << "Median index=" << index << " of total "
              << samples_by_value_.size();
      return sample.first;
    }
    index++;
  }

  // We should only be able to reach here if there are no samples
  DCHECK(samples_by_value_.empty());
  VLOG(2) << "Median 0 (no samples)";
  return 0;
}

}  // namespace util
}  // namespace ndash
