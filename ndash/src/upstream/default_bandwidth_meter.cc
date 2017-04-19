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

#include "upstream/default_bandwidth_meter.h"

#include <cstdint>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/task_runner.h"
#include "base/time/default_tick_clock.h"
#include "upstream/constants.h"
#include "util/sliding_median.h"

namespace ndash {
namespace upstream {

constexpr int32_t DefaultBandwidthMeter::kDefaultMaxWeight;

DefaultBandwidthMeter::DefaultBandwidthMeter(
    BandwidthSampleCB sample_cb,
    scoped_refptr<base::TaskRunner> cb_task_runner,
    int32_t max_weight)
    : DefaultBandwidthMeter(
          std::move(sample_cb),
          std::move(cb_task_runner),
          base::WrapUnique(new base::DefaultTickClock),
          base::WrapUnique(new util::SlidingMedian(max_weight))) {}

DefaultBandwidthMeter::DefaultBandwidthMeter(
    BandwidthSampleCB sample_cb,
    scoped_refptr<base::TaskRunner> cb_task_runner,
    std::unique_ptr<base::TickClock> clock,
    std::unique_ptr<util::AveragerInterface> averager)
    : sample_cb_(std::move(sample_cb)),
      cb_task_runner_(std::move(cb_task_runner)),
      clock_(std::move(clock)),
      averager_(std::move(averager)) {
  DCHECK(sample_cb.is_null() || cb_task_runner);
}

DefaultBandwidthMeter::~DefaultBandwidthMeter() {}

int64_t DefaultBandwidthMeter::GetBitrateEstimate() const {
  base::AutoLock auto_lock(lock_);
  // TODO(adewhurst): Consider making the bitrate estimate an atomic value and
  //                  avoid locking. No-barrier loads/stores should be fine.
  return bitrate_estimate_;
}

void DefaultBandwidthMeter::OnTransferStart() {
  base::AutoLock auto_lock(lock_);

  VLOG(2) << "Transfer Start stream_count=" << stream_count_;

  if (stream_count_++ == 0) {
    start_time_ = clock_->NowTicks();
    DCHECK_EQ(bytes_accumulator_, 0);
  }
}

void DefaultBandwidthMeter::OnBytesTransferred(int32_t bytes) {
  DCHECK_GT(bytes, 0);
  VLOG(3) << "Transferred: " << bytes;

  base::AutoLock auto_lock(lock_);
  // TODO(adewhurst): Consider making the accumulator atomic. We just increment
  //                  it here (no barrier required) and then read it in
  //                  OnTransferEnd().
  bytes_accumulator_ += bytes;
}

void DefaultBandwidthMeter::OnTransferEnd() {
  base::TimeDelta zero_time;
  base::TimeDelta elapsed;
  int64_t accumulator;
  int64_t bandwidth_estimate = kNoEstimate;
  bool new_estimate = false;

  {
    base::AutoLock auto_lock(lock_);
    DCHECK_GT(stream_count_, 0);

    base::TimeTicks now = clock_->NowTicks();
    elapsed = now - start_time_;
    accumulator = bytes_accumulator_;

    if (elapsed > zero_time && accumulator > 0) {
      int64_t bits_per_second = accumulator * kBitsPerByte *
                                base::Time::kMicrosecondsPerSecond /
                                elapsed.InMicroseconds();
      averager_->AddSample(sqrt(accumulator), bits_per_second);
      bandwidth_estimate = averager_->GetAverage();

      if (bandwidth_estimate <= 0) {
        bandwidth_estimate = kNoEstimate;
      }

      new_estimate = true;
      bitrate_estimate_ = bandwidth_estimate;
    }

    --stream_count_;
    start_time_ = now;
    bytes_accumulator_ = 0;

    VLOG(2) << "Transfer End stream_count=" << stream_count_;
  }

  if (new_estimate) {
    VLOG(1) << "New estimate: elapsed=" << elapsed << " bytes=" << accumulator
            << " bitrate=" << bandwidth_estimate;

    NotifyBandwidthSample(elapsed, accumulator, bandwidth_estimate);
  }
}

void DefaultBandwidthMeter::NotifyBandwidthSample(base::TimeDelta elapsed,
                                                  int64_t bytes,
                                                  int64_t bitrate) {
  if (sample_cb_.is_null())
    return;

  DCHECK(cb_task_runner_);

  base::Closure task = base::Bind(sample_cb_, elapsed, bytes, bitrate);
  cb_task_runner_->PostTask(FROM_HERE, task);
}

}  // namespace upstream
}  // namespace ndash
