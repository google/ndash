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

#include "chunk/adaptive_evaluator.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <memory>
#include <vector>

#include "base/logging.h"
#include "base/time/time.h"
#include "chunk/chunk.h"
#include "chunk/format_evaluator.h"
#include "chunk/media_chunk.h"
#include "playback_rate.h"
#include "upstream/bandwidth_meter.h"
#include "util/format.h"

namespace ndash {
namespace chunk {

namespace {
constexpr int32_t kDefaultMaxInitialBitrate = 16000000;  // 16mbps

constexpr int kDefaultMinDurationForQualityIncreaseMs = 10000;
constexpr int kDefaultMaxDurationForQualityDecreaseMs = 25000;
constexpr int kDefaultMinDurationToRetainAfterDiscardMs = 15000;
// 90% to account for audio+text consuming some of the bandwidth
constexpr float kDefaultBandwidthFraction = 0.90f;

constexpr int kMinHdHeight = 720;
constexpr int kMinHdWidth = 1280;
}  // namespace

AdaptiveEvaluator::AdaptiveEvaluator(
    const upstream::BandwidthMeterInterface* bandwidth_meter)
    : AdaptiveEvaluator(bandwidth_meter,
                        kDefaultMaxInitialBitrate,
                        base::TimeDelta::FromMilliseconds(
                            kDefaultMinDurationForQualityIncreaseMs),
                        base::TimeDelta::FromMilliseconds(
                            kDefaultMaxDurationForQualityDecreaseMs),
                        base::TimeDelta::FromMilliseconds(
                            kDefaultMinDurationToRetainAfterDiscardMs),
                        kDefaultBandwidthFraction) {}

AdaptiveEvaluator::AdaptiveEvaluator(
    const upstream::BandwidthMeterInterface* bandwidth_meter,
    int32_t max_initial_bitrate,
    base::TimeDelta min_duration_for_quality_increase,
    base::TimeDelta max_duration_for_quality_decrease,
    base::TimeDelta min_duration_to_retain_after_discard,
    float bandwidth_fraction)
    : bandwidth_meter_(bandwidth_meter),
      max_initial_bitrate_(max_initial_bitrate),
      min_duration_for_quality_increase_(min_duration_for_quality_increase),
      max_duration_for_quality_decrease_(max_duration_for_quality_decrease),
      min_duration_to_retain_after_discard_(
          min_duration_to_retain_after_discard),
      bandwidth_fraction_(bandwidth_fraction) {}

AdaptiveEvaluator::~AdaptiveEvaluator() {}

void AdaptiveEvaluator::Enable() {}
void AdaptiveEvaluator::Disable() {}

void AdaptiveEvaluator::Evaluate(
    const std::deque<std::unique_ptr<MediaChunk>>& queue,
    base::TimeDelta playback_position,
    const std::vector<util::Format>& formats,
    FormatEvaluation* evaluation,
    const PlaybackRate& playback_rate) const {
  DCHECK(!formats.empty());

  base::TimeDelta buffered_duration;
  if (!queue.empty()) {
    buffered_duration =
        base::TimeDelta::FromMicroseconds(queue.back()->end_time_us()) -
        playback_position;
  }

  const std::unique_ptr<util::Format>& current(evaluation->format_);
  const util::Format* ideal = DetermineIdealFormat(
      formats, bandwidth_meter_->GetBitrateEstimate(), playback_rate);
  bool is_higher =
      ideal && current && ideal->GetBitrate() > current->GetBitrate();
  bool is_lower =
      ideal && current && ideal->GetBitrate() < current->GetBitrate();
  if (is_higher) {
    if (buffered_duration < min_duration_for_quality_increase_) {
      // The ideal format is a higher quality, but we have insufficient buffer
      // to safely switch up. Defer switching up for now.
      ideal = current.get();
      VLOG(1) << "Evaluation: ideal > current, but not enough buffer";
    } else if (buffered_duration >= min_duration_to_retain_after_discard_) {
      VLOG(1) << "Evaluation: ideal > current, discarding extra buffer";
      // We're switching from an SD stream to a stream of higher resolution.
      // Consider discarding already buffered media chunks. Specifically,
      // discard media chunks starting from the first one that is of lower
      // bandwidth, lower resolution and that is not HD.
      for (size_t i = 1; i < queue.size(); i++) {
        const MediaChunk& this_chunk = *queue.at(i);
        base::TimeDelta duration_before_this_segment =
            base::TimeDelta::FromMicroseconds(this_chunk.start_time_us()) -
            playback_position;
        if (duration_before_this_segment >=
                min_duration_to_retain_after_discard_ &&
            this_chunk.format()->GetBitrate() < ideal->GetBitrate() &&
            this_chunk.format()->GetHeight() < ideal->GetHeight() &&
            this_chunk.format()->GetHeight() < kMinHdHeight &&
            this_chunk.format()->GetWidth() < kMinHdWidth) {
          // Discard chunks from this one onwards.
          evaluation->queue_size_ = i;
          break;
        }
      }
    } else {
      VLOG(1) << "Evaluation: ideal > current";
    }
  } else if (is_lower) {
    if (buffered_duration >= max_duration_for_quality_decrease_) {
      // The ideal format is a lower quality, but we have sufficient buffer to
      // defer switching down for now.
      VLOG(1) << "Evaluation: ideal < current but buffer is sufficient";
      ideal = current.get();
    } else {
      VLOG(1) << "Evaluation: ideal < current";
    }
  }
  CHECK(ideal);
  if (current) {
    evaluation->trigger_ = Chunk::kTriggerAdaptive;
  }
  if (current.get() != ideal) {
    // TODO(adewhurst): See if we can avoid making a copy and always just return
    //                  a pointer into the formats vector. That may require
    //                  some care at period boundaries. Also see if we can use
    //                  *current == *ideal to avoid copies when the same format
    //                  is selected (the pointers won't be equal in that case
    //                  due to the copy)
    VLOG(2) << "Evaulation: changed (old bitrate "
            << (current ? current->GetBitrate() : -1) << ", new bitrate "
            << ideal->GetBitrate() << ")";
    evaluation->format_.reset(new util::Format(*ideal));
  } else {
    VLOG(2) << "Evaluation: no change";
  }
}

int64_t AdaptiveEvaluator::EffectiveBitrate(int64_t bitrate_estimate) const {
  return bitrate_estimate == upstream::BandwidthMeterInterface::kNoEstimate
             ? max_initial_bitrate_
             : llrint(bitrate_estimate * bandwidth_fraction_);
}

const util::Format* AdaptiveEvaluator::DetermineIdealFormat(
    const std::vector<util::Format>& formats,
    int64_t effective_bitrate,
    const PlaybackRate& playback_rate) {
  std::unique_ptr<util::Format> result;

  if (VLOG_IS_ON(5)) {
    VLOG(5) << "Formats dump start";
    for (const util::Format& format : formats) {
      VLOG(5) << "bitrate " << format.GetBitrate();
    }
    VLOG(5) << "Formats dump done";
  }

  CHECK(!formats.empty());

  // Filter the formats by max playout rate first
  std::vector<const util::Format*> filtered_formats;
  filtered_formats.reserve(formats.size());

  for (const util::Format& format : formats) {
    if (filtered_formats.empty()) {
      // We have to start somewhere
      filtered_formats.push_back(&format);
      continue;
    }

    float filter_rate = filtered_formats.front()->GetMaxPlayoutRate();
    float current_rate = format.GetMaxPlayoutRate();

    if (filter_rate == current_rate) {
      // Since a format matching the playout rate of the current format already
      // passed the filter, we should collect this one too, even if it's not
      // the best in the end (the filtered list will be cleared if a format
      // with a better playout rate is found).
      filtered_formats.push_back(&format);
      continue;
    }

    // After this point, if we decide to use the current format, we clear
    // filtered_formats first because the rates do not match.

    if (filter_rate < playback_rate.abs_rate()) {
      // The previously filtered format's max playout rate is too low
      if (current_rate > filter_rate) {
        // ... and the current format is better, so we throw the old formats
        // away. The current one might still be too slow or too fast, but it's
        // still better.
        filtered_formats.clear();
        filtered_formats.push_back(&format);
      }
      // Otherwise, the current format is worse so we don't let it past the
      // filter.
      continue;
    }

    if (current_rate >= playback_rate.abs_rate() &&
        current_rate < filter_rate) {
      // The current format's max playout rate satisfies the current playback
      // rate, and is better than what the filter previously found.
      filtered_formats.clear();
      filtered_formats.push_back(&format);
    }
    // Otherwise, the current format's max playout rate is too high vs what is
    // already in the filtered list.
  }

  // Search for the best format. We perform a linear scan. ExoPlayer's
  // FormatEvaluator interface required that the formats be passed in
  // descending bandwidth order, but they are not necessarily sorted in the
  // manifest, which meant that we were going to have to sort the formats each
  // time before calling this method (a side effect of the code to handle
  // different representations per period).
  //
  // Rather than an O(n log n) sort followed by a O(log n) binary search for
  // the best format, we simply settle on an O(n) search, which is an overall
  // win. Also, the number of formats should never grow too large anyway.

  const util::Format* best_so_far = nullptr;

  for (const util::Format* format : filtered_formats) {
    if (!best_so_far ||
        // In case we can't find any format below effective_bitrate, find the
        // lowest bitrate available
        (best_so_far->GetBitrate() > effective_bitrate &&
         format->GetBitrate() < best_so_far->GetBitrate()) ||
        // Otherwise, we want the highest bitrate that does not go over
        // effective_bitrate
        (format->GetBitrate() <= effective_bitrate &&
         format->GetBitrate() > best_so_far->GetBitrate())) {
      best_so_far = format;
    }
  }

  return best_so_far;
}

}  // namespace chunk
}  // namespace ndash
