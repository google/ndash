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

#ifndef NDASH_UPSTREAM_DEFAULT_BANDWIDTH_METER_H_
#define NDASH_UPSTREAM_DEFAULT_BANDWIDTH_METER_H_

#include <cstdint>
#include <memory>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/task_runner.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "upstream/bandwidth_meter.h"
#include "upstream/transfer_listener.h"
#include "util/averager.h"

namespace ndash {
namespace upstream {

// Counts transferred bytes while transfers are open and creates a bandwidth
// sample and updated bandwidth estimate each time a transfer ends.
class DefaultBandwidthMeter : public BandwidthMeterInterface,
                              public TransferListenerInterface {
 public:
  constexpr static int32_t kDefaultMaxWeight = 20000;

  DefaultBandwidthMeter(
      BandwidthSampleCB sample_cb = BandwidthSampleCB(),
      scoped_refptr<base::TaskRunner> cb_task_runner = nullptr,
      int32_t max_weight = kDefaultMaxWeight);

  // Expanded constructor with dependency injection for testing
  DefaultBandwidthMeter(BandwidthSampleCB sample_cb,
                        scoped_refptr<base::TaskRunner> cb_task_runner,
                        std::unique_ptr<base::TickClock> clock,
                        std::unique_ptr<util::AveragerInterface> averager);

  ~DefaultBandwidthMeter() override;

  // May be called by any thread
  int64_t GetBitrateEstimate() const override;

  // Will be called by loader thread(s); locking required
  void OnTransferStart() override;
  void OnBytesTransferred(int32_t bytes) override;
  void OnTransferEnd() override;

 private:
  DefaultBandwidthMeter(const DefaultBandwidthMeter& other) = delete;
  DefaultBandwidthMeter& operator=(const DefaultBandwidthMeter& other) = delete;

  // Only accesses const members, so can run without locking
  void NotifyBandwidthSample(base::TimeDelta elapsed,
                             int64_t bytes,
                             int64_t bitrate);

  const BandwidthSampleCB sample_cb_;
  const scoped_refptr<base::TaskRunner> cb_task_runner_;
  const std::unique_ptr<base::TickClock> clock_;

  mutable base::Lock lock_;  // Protects all of the mutable members

  std::unique_ptr<util::AveragerInterface> averager_;
  int64_t bytes_accumulator_ = 0;
  base::TimeTicks start_time_;
  int64_t bitrate_estimate_ = kNoEstimate;
  int stream_count_ = 0;
};

}  // namespace upstream
}  // namespace ndash

#endif  // NDASH_UPSTREAM_DEFAULT_BANDWIDTH_METER_H_
