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

#ifndef NDASH_UPSTREAM_BANDWIDTH_METER_H_
#define NDASH_UPSTREAM_BANDWIDTH_METER_H_

#include <cstdint>

#include "base/callback.h"
#include "base/time/time.h"

namespace ndash {
namespace upstream {

// Provides estimates of the currently available bandwidth.
class BandwidthMeterInterface {
 public:
  typedef base::Callback<
      void(base::TimeDelta elapsed, int64_t bytes, int64_t bitrate)>
      BandwidthSampleCB;

  virtual ~BandwidthMeterInterface() {}

  // Indicates no bandwidth estimate is available.
  constexpr static int64_t kNoEstimate = -1;

  // Returns the estimated bandwidth in bits/sec, or kNoEstimate if no estimate
  // is available.
  virtual int64_t GetBitrateEstimate() const = 0;

 protected:
  BandwidthMeterInterface() {}

  BandwidthMeterInterface(const BandwidthMeterInterface& other) = delete;
  BandwidthMeterInterface& operator=(const BandwidthMeterInterface& other) =
      delete;
};

}  // namespace upstream
}  // namespace ndash

#endif  // NDASH_UPSTREAM_BANDWIDTH_METER_H_
