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

#ifndef NDASH_CHUNK_ADAPTIVE_EVALUATOR_H_
#define NDASH_CHUNK_ADAPTIVE_EVALUATOR_H_

#include <deque>
#include <memory>
#include <vector>

#include "base/time/time.h"
#include "chunk/chunk.h"
#include "chunk/format_evaluator.h"
#include "util/format.h"

namespace ndash {

class PlaybackRate;

namespace upstream {
class BandwidthMeterInterface;
}  // namespace upstream

namespace chunk {

class MediaChunk;

class AdaptiveEvaluator : public FormatEvaluatorInterface {
 public:
  // bandwidth_meter: Provides an estimate of the currently available bandwidth.
  AdaptiveEvaluator(const upstream::BandwidthMeterInterface* bandwidth_meter);

  // bandwidthMeter Provides an estimate of the currently available bandwidth.
  // max_initial_bitrate: The maximum bitrate in bits per second that should
  //                      be assumed when bandwidth_meter cannot provide an
  //                      estimate due to playback having only just started.
  // min_duration_for_quality_increase: The minimum duration of buffered data
  //                                    required for the evaluator to
  //                                    consider switching to a higher
  //                                    quality format.
  // max_duration_for_quality_decrease: The maximum duration of buffered data
  //                                    required for the evaluator to
  //                                    consider switching to a lower quality
  //                                    format.
  // min_duration_to_retain_after_discard: When switching to a significantly
  //                                       higher quality format, the
  //                                       evaluator may discard some of the
  //                                       media that it has already buffered
  //                                       at the lower quality, so as to
  //                                       switch up to the higher quality
  //                                       faster. This is the minimum
  //                                       duration of media that must be
  //                                       retained at the lower quality.
  // bandwidth_fraction: The fraction of the available bandwidth that the
  //                     evaluator should consider available for use. Setting
  //                     to a value less than 1 is recommended to account for
  //                     inaccuracies in the bandwidth estimator.
  AdaptiveEvaluator(const upstream::BandwidthMeterInterface* bandwidth_meter,
                    int32_t max_initial_bitrate,
                    base::TimeDelta min_duration_for_quality_increase,
                    base::TimeDelta max_duration_for_quality_decrease,
                    base::TimeDelta min_duration_to_retain_after_discard,
                    float bandwidth_fraction);

  ~AdaptiveEvaluator() override;

  void Enable() override;
  void Disable() override;
  void Evaluate(const std::deque<std::unique_ptr<MediaChunk>>& queue,
                base::TimeDelta playback_position,
                const std::vector<util::Format>& formats,
                FormatEvaluation* evaluation,
                const PlaybackRate& playback_rate) const override;

 private:
  friend class AdaptiveEvaluatorTest;

  int64_t EffectiveBitrate(int64_t bitrate_estimate) const;

  // Find the ideal format (within |formats|), ignoring buffer health.
  static const util::Format* DetermineIdealFormat(
      const std::vector<util::Format>& formats,
      int64_t effective_bitrate,
      const PlaybackRate& playback_rate);

  const upstream::BandwidthMeterInterface* bandwidth_meter_;

  int32_t max_initial_bitrate_;
  base::TimeDelta min_duration_for_quality_increase_;
  base::TimeDelta max_duration_for_quality_decrease_;
  base::TimeDelta min_duration_to_retain_after_discard_;
  float bandwidth_fraction_;
};

}  // namespace chunk
}  // namespace ndash

#endif  // NDASH_CHUNK_ADAPTIVE_EVALUATOR_H_
