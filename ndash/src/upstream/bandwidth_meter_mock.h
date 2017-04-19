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

#ifndef NDASH_UPSTREAM_BANDWIDTH_METER_MOCK_H_
#define NDASH_UPSTREAM_BANDWIDTH_METER_MOCK_H_

#include <cstdint>

#include "gmock/gmock.h"
#include "upstream/bandwidth_meter.h"

namespace ndash {
namespace upstream {

class MockBandwidthMeter : public BandwidthMeterInterface {
 public:
  MockBandwidthMeter();
  ~MockBandwidthMeter() override;

  MOCK_CONST_METHOD0(GetBitrateEstimate, int64_t());

 protected:
  MockBandwidthMeter(const MockBandwidthMeter& other) = delete;
  MockBandwidthMeter& operator=(const MockBandwidthMeter& other) = delete;
};

}  // namespace upstream
}  // namespace ndash

#endif  // NDASH_UPSTREAM_BANDWIDTH_METER_MOCK_H_
