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

#include "chunk/demo_evaluator.h"

#include <algorithm>

#include "base/logging.h"
#include "playback_rate.h"
#include "util/mime_types.h"

namespace ndash {
namespace chunk {

namespace {
// A comparator class that will sort formats first by max playout rate in
// descending order (if |descending_mpor| is true), then by bitrate in
// ascending order.  Otherwise, if |descending_mpor| is false, both max
// playout rate and bitrate are in ascending order.
class DemoFormatComparator {
 public:
  explicit DemoFormatComparator(bool descending_mpor = true)
      : descending_mpor_(descending_mpor) {}

  bool operator()(const util::Format* f1, const util::Format* f2) {
    int32_t f1MaxPlayoutRate = f1->GetMaxPlayoutRate();
    int32_t f1BitRate = f1->GetBitrate();
    int32_t f2MaxPlayoutRate = f2->GetMaxPlayoutRate();
    int32_t f2BitRate = f2->GetBitrate();

    if (descending_mpor_) {
      // Descending max playout rates
      // Ascending bitrates
      return std::tie(f2MaxPlayoutRate, f1BitRate, f2->GetId()) <
             std::tie(f1MaxPlayoutRate, f2BitRate, f1->GetId());
    } else {
      // Ascending max playout rates
      // Ascending bitrates
      return std::tie(f1MaxPlayoutRate, f1BitRate, f2->GetId()) <
             std::tie(f2MaxPlayoutRate, f2BitRate, f1->GetId());
    }
  }

 private:
  bool descending_mpor_;
};

// Picks the highest bitrate format among the formats that have the
// lowest max playout rate that is greater than the playback rate.
const util::Format* SelectFormat(const std::vector<util::Format>& formats,
                                 const PlaybackRate& playback_rate) {
  std::vector<const util::Format*> gte_fmts;
  gte_fmts.reserve(formats.size());
  std::vector<const util::Format*> lt_fmts;
  lt_fmts.reserve(formats.size());
  for (auto& f : formats) {
    // gte_fmts gets anything that is >= our playback rate.
    if (f.GetMaxPlayoutRate() >= playback_rate.GetMagnitude()) {
      gte_fmts.push_back(&f);
    } else {
      lt_fmts.push_back(&f);
    }
  }

  // TODO(rmrossi): For demo evaluator, we'll always pick the highest bitrate.
  // Make sure when adaptive selector is used, it also honors the same
  // logic on the max playout rate.
  if (gte_fmts.empty()) {
    // No formats had a max playout rate >= our playback rate. The best we
    // can do is use something smaller.
    if (lt_fmts.empty()) {
      return nullptr;
    }
    // Choose the track with the highest max playout rate.
    DemoFormatComparator comp(false);
    return *std::max_element(std::begin(lt_fmts), std::end(lt_fmts), comp);
  }

  // Choose the track with the highest playout rate that does not go below
  // our playback rate.
  DemoFormatComparator comp;
  return *std::max_element(std::begin(gte_fmts), std::end(gte_fmts), comp);
}

void EvaluateVideo(const std::vector<util::Format>& formats,
                   FormatEvaluation* evaluation,
                   const PlaybackRate& playback_rate) {
  const util::Format* best_format = SelectFormat(formats, playback_rate);
  if (best_format) {
    DCHECK(util::MimeTypes::IsVideo(best_format->GetMimeType()));
    evaluation->format_.reset(new util::Format(*best_format));
  }
}

void EvaluateAudio(const std::vector<util::Format>& formats,
                   FormatEvaluation* evaluation,
                   const PlaybackRate& playback_rate) {
  const util::Format* best_format = SelectFormat(formats, playback_rate);
  if (best_format) {
    DCHECK(util::MimeTypes::IsAudio(best_format->GetMimeType()));
    evaluation->format_.reset(new util::Format(*best_format));
  }
}
}  // namespace

DemoEvaluator::DemoEvaluator() {}
DemoEvaluator::~DemoEvaluator() {}

void DemoEvaluator::Enable() {}
void DemoEvaluator::Disable() {}

void DemoEvaluator::Evaluate(
    const std::deque<std::unique_ptr<MediaChunk>>& queue,
    base::TimeDelta playback_position,
    const std::vector<util::Format>& formats,
    FormatEvaluation* evaluation,
    const PlaybackRate& playback_rate) const {
  CHECK(evaluation);
  CHECK(!formats.empty());

  std::string mime_type = formats[0].GetMimeType();
  if (util::MimeTypes::IsVideo(mime_type)) {
    EvaluateVideo(formats, evaluation, playback_rate);
  } else if (util::MimeTypes::IsAudio(mime_type)) {
    EvaluateAudio(formats, evaluation, playback_rate);
  } else if (util::MimeTypes::IsText(mime_type)) {
    // Can just pick the first one since there is only ever one representation
    // in text tracks.
    const util::Format& best_format = formats.front();
    evaluation->format_.reset(new util::Format(best_format));
  } else {
    LOG(ERROR) << "Unsupported mime type for DemoEvaluator: " << mime_type;
  }
}

}  // namespace chunk
}  // namespace ndash
