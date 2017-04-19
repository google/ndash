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

#include <initializer_list>
#include <memory>

#include "base/time/time.h"
#include "chunk/media_chunk_mock.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "playback_rate.h"
#include "upstream/bandwidth_meter.h"
#include "upstream/bandwidth_meter_mock.h"
#include "upstream/data_spec.h"

namespace ndash {
namespace chunk {

using ::testing::NotNull;
using ::testing::IsNull;
using ::testing::Eq;
using ::testing::Ge;
using ::testing::Le;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::_;

class AdaptiveEvaluatorTest : public ::testing::Test {
 protected:
  AdaptiveEvaluatorTest() {}
  ~AdaptiveEvaluatorTest() override {}

  void VerifyAndClearMocks() { Mock::VerifyAndClearExpectations(&meter_); }

  static const util::Format* DetermineIdealFormat(
      const std::vector<util::Format>& formats,
      int64_t effective_bitrate,
      const PlaybackRate& playback_rate) {
    return AdaptiveEvaluator::DetermineIdealFormat(formats, effective_bitrate,
                                                   playback_rate);
  }

  static int64_t EffectiveBitrate(const AdaptiveEvaluator* evaluator,
                                  int64_t bitrate_estimate) {
    return evaluator->EffectiveBitrate(bitrate_estimate);
  }

  const int32_t kExampleInitialBitrate = 8000000;
  const base::TimeDelta kExampleMinDurationIncrease =
      base::TimeDelta::FromSeconds(10);
  const base::TimeDelta kExampleMaxDurationDecrease =
      base::TimeDelta::FromSeconds(25);
  const base::TimeDelta kExampleMinDurationRetain =
      base::TimeDelta::FromSeconds(25);
  const float kExampleBandwidthFraction = 0.75f;

  StrictMock<upstream::MockBandwidthMeter> meter_;
};

TEST_F(AdaptiveEvaluatorTest, EffectiveBitrate) {
  std::unique_ptr<AdaptiveEvaluator> evaluator;
  constexpr int64_t error_allowed = 150;

  constexpr float kFractionFull = 1.0f;
  evaluator.reset(new AdaptiveEvaluator(
      &meter_, kExampleInitialBitrate, kExampleMinDurationIncrease,
      kExampleMaxDurationDecrease, kExampleMinDurationRetain, kFractionFull));

  // Verify that values come out roughly identical up to ~1.5Gbps. The funny
  // increment is so we aren't just testing round numbers, but it isn't
  // necessary to check every possible value.
  //
  // Error isn't great with large numbers beceause we're using single
  // precision. On the other hand, with large numbers it doesn't matter
  // (highest bitrate stream is low tens of Mbps).

  for (int64_t i = 0; i < 1500000000; i += 876543) {
    int64_t target = i;
    int64_t effective_bitrate = EffectiveBitrate(evaluator.get(), i);
    EXPECT_THAT(effective_bitrate, Ge(target - error_allowed));
    EXPECT_THAT(effective_bitrate, Le(target + error_allowed));
  }
  EXPECT_THAT(EffectiveBitrate(evaluator.get(),
                               upstream::BandwidthMeterInterface::kNoEstimate),
              Eq(kExampleInitialBitrate));

  constexpr float kFractionThreeQuarters = 0.75f;
  evaluator.reset(new AdaptiveEvaluator(
      &meter_, kExampleInitialBitrate, kExampleMinDurationIncrease,
      kExampleMaxDurationDecrease, kExampleMinDurationRetain,
      kFractionThreeQuarters));

  for (int64_t i = 0; i < 1500000000; i += 975319) {
    int64_t target = (i * 3) / 4;
    int64_t effective_bitrate = EffectiveBitrate(evaluator.get(), i);
    EXPECT_THAT(effective_bitrate, Ge(target - error_allowed));
    EXPECT_THAT(effective_bitrate, Le(target + error_allowed));
  }
  EXPECT_THAT(EffectiveBitrate(evaluator.get(),
                               upstream::BandwidthMeterInterface::kNoEstimate),
              Eq(kExampleInitialBitrate));

  constexpr float kFractionHalf = 0.5f;
  evaluator.reset(new AdaptiveEvaluator(
      &meter_, kExampleInitialBitrate, kExampleMinDurationIncrease,
      kExampleMaxDurationDecrease, kExampleMinDurationRetain, kFractionHalf));

  for (int64_t i = 0; i < 1500000000; i += 913977) {
    int64_t target = i / 2;
    int64_t effective_bitrate = EffectiveBitrate(evaluator.get(), i);
    EXPECT_THAT(effective_bitrate, Ge(target - error_allowed));
    EXPECT_THAT(effective_bitrate, Le(target + error_allowed));
  }
  EXPECT_THAT(EffectiveBitrate(evaluator.get(),
                               upstream::BandwidthMeterInterface::kNoEstimate),
              Eq(kExampleInitialBitrate));
}

TEST_F(AdaptiveEvaluatorTest, DetermineIdealFormat) {
  const std::vector<util::Format> formats{
      util::Format("1", "video/x-any", -1, -1, 0.0, -1, -1, -1, 5000),
      util::Format("2", "video/x-any", -1, -1, 0.0, -1, -1, -1, 400),
      util::Format("3", "video/x-any", -1, -1, 0.0, -1, -1, -1, 30),
      util::Format("4", "video/x-any", -1, -1, 0.0, -1, -1, -1, 29),
      util::Format("5", "video/x-any", -1, -1, 0.0, -1, -1, -1, 28),
      util::Format("6", "video/x-any", -1, -1, 0.0, -1, -1, -1, 5),
  };

  ASSERT_THAT(std::is_sorted(formats.begin(), formats.end(),
                             util::Format::DecreasingBandwidthComparator()),
              Eq(true));

  PlaybackRate rate;

  EXPECT_THAT(DetermineIdealFormat(formats, 10000, rate), Eq(&formats[0]));
  EXPECT_THAT(DetermineIdealFormat(formats, 5001, rate), Eq(&formats[0]));
  EXPECT_THAT(DetermineIdealFormat(formats, 5000, rate), Eq(&formats[0]));
  EXPECT_THAT(DetermineIdealFormat(formats, 4999, rate), Eq(&formats[1]));
  EXPECT_THAT(DetermineIdealFormat(formats, 2000, rate), Eq(&formats[1]));
  EXPECT_THAT(DetermineIdealFormat(formats, 401, rate), Eq(&formats[1]));
  EXPECT_THAT(DetermineIdealFormat(formats, 400, rate), Eq(&formats[1]));
  EXPECT_THAT(DetermineIdealFormat(formats, 399, rate), Eq(&formats[2]));
  EXPECT_THAT(DetermineIdealFormat(formats, 31, rate), Eq(&formats[2]));
  EXPECT_THAT(DetermineIdealFormat(formats, 31, rate), Eq(&formats[2]));
  EXPECT_THAT(DetermineIdealFormat(formats, 30, rate), Eq(&formats[2]));
  EXPECT_THAT(DetermineIdealFormat(formats, 29, rate), Eq(&formats[3]));
  EXPECT_THAT(DetermineIdealFormat(formats, 28, rate), Eq(&formats[4]));
  EXPECT_THAT(DetermineIdealFormat(formats, 27, rate), Eq(&formats[5]));
  EXPECT_THAT(DetermineIdealFormat(formats, 5, rate), Eq(&formats[5]));
  EXPECT_THAT(DetermineIdealFormat(formats, 1, rate), Eq(&formats[5]));
  EXPECT_THAT(DetermineIdealFormat(formats, 0, rate), Eq(&formats[5]));
  EXPECT_THAT(DetermineIdealFormat(formats, -1, rate), Eq(&formats[5]));
  EXPECT_THAT(DetermineIdealFormat(formats, -1000000, rate), Eq(&formats[5]));
}

TEST_F(AdaptiveEvaluatorTest, Evaluate) {
  const upstream::DataSpec data_spec(upstream::Uri(""));
  constexpr int32_t kChunkIndex = 42;

  const std::vector<util::Format> formats{
      util::Format("hd_low", "video/x-any", 1280, 720, 0.0, -1, -1, -1, 400),
      util::Format("sd_midhigh", "video/x-any", 720, 480, 0.0, -1, -1, -1, 29),
      util::Format("sd_low", "video/x-any", 320, 240, 0.0, -1, -1, -1, 5),
      util::Format("hd_high", "video/x-any", 1920, 1080, 0.0, -1, -1, -1, 5000),
      util::Format("sd_high", "video/x-any", 960, 576, 0.0, -1, -1, -1, 30),
      util::Format("sd_mid", "video/x-any", 640, 480, 0.0, -1, -1, -1, 28),
  };

  const util::Format& hd_high_format = formats.at(3);
  EXPECT_THAT(hd_high_format.GetId(), Eq("hd_high"));

  const util::Format& hd_low_format = formats.at(0);
  EXPECT_THAT(hd_low_format.GetId(), Eq("hd_low"));

  const util::Format& middle_format = formats.at(5);
  EXPECT_THAT(middle_format.GetId(), Eq("sd_mid"));

  const std::deque<std::unique_ptr<MediaChunk>> empty_queue;

  const base::TimeDelta start_playback_time = base::TimeDelta::FromSeconds(1);
  const base::TimeDelta first_chunk_start_time =
      base::TimeDelta::FromSeconds(3);
  const base::TimeDelta chunk_duration = base::TimeDelta::FromSeconds(2);
  constexpr int32_t kChunksAfterDiscard = 12;  // 2s chunks, min time = 25s

  // Queue with 30s of video. This is above the kExampleMaxDurationDecrease and
  // kExmapleMinDurationRetain thresholds.
  // There are two queues: one with a SD format and one with an HD (low) format
  std::deque<std::unique_ptr<MediaChunk>> queue_30s_hd;
  std::deque<std::unique_ptr<MediaChunk>> queue_30s_sd;
  // Queue with 16s of SD video. This is above the kExampleMinDurationIncrease
  // threshold and below the kExampleMinDurationRetain threshold.
  std::deque<std::unique_ptr<MediaChunk>> queue_16s_sd;
  {
    base::TimeDelta current_chunk_start = first_chunk_start_time;

    for (int i = 0; i < 15; i++) {
      base::TimeDelta current_chunk_end = current_chunk_start + chunk_duration;

      queue_30s_hd.emplace_back(new MockMediaChunk(
          &data_spec, Chunk::kTriggerUnspecified, &hd_low_format,
          current_chunk_start.InMicroseconds(),
          current_chunk_end.InMicroseconds(), kChunkIndex + i,
          Chunk::kNoParentId));
      queue_30s_sd.emplace_back(new MockMediaChunk(
          &data_spec, Chunk::kTriggerUnspecified, &middle_format,
          current_chunk_start.InMicroseconds(),
          current_chunk_end.InMicroseconds(), kChunkIndex + i,
          Chunk::kNoParentId));
      if (i < 8) {
        queue_16s_sd.emplace_back(new MockMediaChunk(
            &data_spec, Chunk::kTriggerUnspecified, &middle_format,
            current_chunk_start.InMicroseconds(),
            current_chunk_end.InMicroseconds(), kChunkIndex + i,
            Chunk::kNoParentId));
      }

      VLOG(2) << "Chunk " << i << ": [" << current_chunk_start << ", "
              << current_chunk_end << "]";

      current_chunk_start = current_chunk_end;
    }
  }

  AdaptiveEvaluator evaluator(
      &meter_, kExampleInitialBitrate, kExampleMinDurationIncrease,
      kExampleMaxDurationDecrease, kExampleMinDurationRetain,
      kExampleBandwidthFraction);

  FormatEvaluation evaluation;
  PlaybackRate rate;

  EXPECT_CALL(meter_, GetBitrateEstimate())
      .WillRepeatedly(Return(middle_format.GetBitrate()));

  evaluator.Evaluate(empty_queue, start_playback_time, formats, &evaluation,
                     rate);

  ASSERT_THAT(evaluation.format_, NotNull());
  EXPECT_THAT(*evaluation.format_, Eq(middle_format));
  EXPECT_THAT(evaluation.queue_size_, Eq(0));
  EXPECT_THAT(evaluation.trigger_, Eq(Chunk::kTriggerInitial));

  // Check that trigger changes to adaptive
  evaluator.Evaluate(empty_queue, start_playback_time, formats, &evaluation,
                     rate);

  ASSERT_THAT(evaluation.format_, NotNull());
  EXPECT_THAT(*evaluation.format_, Eq(middle_format));
  EXPECT_THAT(evaluation.queue_size_, Eq(0));
  EXPECT_THAT(evaluation.trigger_, Eq(Chunk::kTriggerAdaptive));

  VerifyAndClearMocks();

  VLOG(1) << "For higher bitrates, trigger the \"ideal > current, but not "
          << "enough buffer\" case; for lower bitrates trigger the \"ideal < "
          << "current\" case.";
  for (const util::Format& format : formats) {
    // Reset to a middle initial format
    evaluation.format_.reset(new util::Format(middle_format));
    evaluation.trigger_ = Chunk::kTriggerInitial;

    // Select a bitrate from one of the formats
    EXPECT_CALL(meter_, GetBitrateEstimate())
        .WillRepeatedly(Return(format.GetBitrate()));

    bool is_lower = format.GetBitrate() < evaluation.format_->GetBitrate();

    evaluator.Evaluate(empty_queue, start_playback_time, formats, &evaluation,
                       rate);

    ASSERT_THAT(evaluation.format_, NotNull());
    if (is_lower) {
      VLOG(1) << "Lowered the bitrate";
      EXPECT_THAT(*evaluation.format_, Eq(format));
    } else {
      VLOG(1) << "Estimated bitrate >= current, but buffer is too small to "
              << "jump up";
      EXPECT_THAT(*evaluation.format_, Eq(middle_format));
    }

    EXPECT_THAT(evaluation.queue_size_, Eq(0));
    EXPECT_THAT(evaluation.trigger_, Eq(Chunk::kTriggerAdaptive));

    VerifyAndClearMocks();
  }

  VLOG(1) << "Trigger the \"ideal > current\" case";
  EXPECT_CALL(meter_, GetBitrateEstimate())
      .WillRepeatedly(Return(hd_low_format.GetBitrate()));

  evaluation.format_.reset(new util::Format(middle_format));
  evaluation.queue_size_ = queue_16s_sd.size();
  evaluation.trigger_ = Chunk::kTriggerInitial;

  evaluator.Evaluate(queue_16s_sd, start_playback_time, formats, &evaluation,
                     rate);

  ASSERT_THAT(evaluation.format_, NotNull());
  EXPECT_THAT(*evaluation.format_, Eq(hd_low_format));
  EXPECT_THAT(evaluation.queue_size_, Eq(queue_16s_sd.size()));
  EXPECT_THAT(evaluation.trigger_, Eq(Chunk::kTriggerAdaptive));

  VerifyAndClearMocks();

  VLOG(1) << "Trigger the \"ideal > current, discarding extra buffer\" case";
  EXPECT_CALL(meter_, GetBitrateEstimate())
      .WillRepeatedly(Return(hd_high_format.GetBitrate()));

  evaluation.format_.reset(new util::Format(middle_format));
  evaluation.queue_size_ = queue_30s_sd.size();
  evaluation.trigger_ = Chunk::kTriggerInitial;

  evaluator.Evaluate(queue_30s_sd, start_playback_time, formats, &evaluation,
                     rate);

  ASSERT_THAT(evaluation.format_, NotNull());
  EXPECT_THAT(*evaluation.format_, Eq(hd_high_format));
  EXPECT_THAT(evaluation.queue_size_, Eq(kChunksAfterDiscard));
  EXPECT_THAT(evaluation.trigger_, Eq(Chunk::kTriggerAdaptive));

  VerifyAndClearMocks();

  VLOG(1) << "Trigger the \"ideal < current but buffer is sufficient\" case";
  EXPECT_CALL(meter_, GetBitrateEstimate())
      .WillRepeatedly(Return(middle_format.GetBitrate()));

  evaluation.format_.reset(new util::Format(hd_high_format));
  evaluation.queue_size_ = queue_30s_hd.size();
  evaluation.trigger_ = Chunk::kTriggerInitial;

  evaluator.Evaluate(queue_30s_hd, start_playback_time, formats, &evaluation,
                     rate);

  ASSERT_THAT(evaluation.format_, NotNull());
  EXPECT_THAT(*evaluation.format_, Eq(hd_high_format));
  EXPECT_THAT(evaluation.queue_size_, Eq(queue_30s_hd.size()));
  EXPECT_THAT(evaluation.trigger_, Eq(Chunk::kTriggerAdaptive));
}

TEST_F(AdaptiveEvaluatorTest, TestEvaluateVideoMaxPlayoutRate) {
  upstream::DataSpec data_spec(upstream::Uri("dummy://"));
  PlaybackRate playback_rate;

  std::vector<util::Format> formats;
  // MaxPlayoutRate 1
  util::Format format1_low("1_low", "video/mp4", 1280, 720, 20.0, 1, 2, 2345,
                           5000);
  util::Format format1_mid("1_mid", "video/mp4", 640, 480, 10.0, 1, 1, 1234,
                           10000);
  util::Format format1_hi("1_hi", "video/mp4", 1920, 1080, 30.0, 1, 3, 3456,
                          20000);

  // MaxPlayoutRate 8
  util::Format format8_low("8_low", "video/mp4", 1280, 720, 20.0, 8, 2, 2345,
                           5000);
  util::Format format8_mid("8_mid", "video/mp4", 640, 480, 10.0, 8, 1, 1234,
                           10000);
  util::Format format8_hi("8_hi", "video/mp4", 1920, 1080, 30.0, 8, 3, 3456,
                          20000);

  // MaxPlayoutRate 16
  util::Format format16_low("16_low", "video/mp4", 1280, 720, 20.0, 16, 2, 2345,
                            5000);
  util::Format format16_mid("16_mid", "video/mp4", 640, 480, 10.0, 16, 1, 1234,
                            10000);
  util::Format format16_hi("16_hi", "video/mp4", 1920, 1080, 30.0, 16, 3, 3456,
                           20000);

  formats.push_back(format1_low);
  formats.push_back(format1_mid);
  formats.push_back(format1_hi);
  formats.push_back(format8_mid);
  formats.push_back(format8_hi);
  formats.push_back(format8_low);
  formats.push_back(format16_mid);
  formats.push_back(format16_low);
  formats.push_back(format16_hi);

  util::Format* first_format = &formats.at(0);
  EXPECT_THAT(*first_format, Eq(format1_low));

  std::deque<std::unique_ptr<MediaChunk>> queue;
  base::TimeDelta playback_position(base::TimeDelta::FromSeconds(0));

  AdaptiveEvaluator evaluator(
      &meter_, kExampleInitialBitrate, kExampleMinDurationIncrease,
      kExampleMaxDurationDecrease, kExampleMinDurationRetain,
      kExampleBandwidthFraction);
  evaluator.Enable();

  EXPECT_CALL(meter_, GetBitrateEstimate())
      .WillRepeatedly(Return(format1_hi.GetBitrate()));

  // With a playback rate of 1, we should choose format 1_hi.
  {
    playback_rate.set_rate(1);
    FormatEvaluation evaluation;
    evaluator.Evaluate(queue, playback_position, formats, &evaluation,
                       playback_rate);
    EXPECT_THAT(*evaluation.format_, Eq(format1_hi));
    EXPECT_THAT(evaluation.trigger_, Eq(Chunk::kTriggerInitial));
  }

  // With a playback rate of 8, we should choose format 8_hi.
  {
    playback_rate.set_rate(8);
    FormatEvaluation evaluation;
    evaluator.Evaluate(queue, playback_position, formats, &evaluation,
                       playback_rate);
    EXPECT_THAT(*evaluation.format_, Eq(format8_hi));
    EXPECT_THAT(evaluation.trigger_, Eq(Chunk::kTriggerInitial));
  }

  // With a playback rate of -8, we should choose format 8_hi.
  {
    playback_rate.set_rate(-8);
    FormatEvaluation evaluation;
    evaluator.Evaluate(queue, playback_position, formats, &evaluation,
                       playback_rate);
    EXPECT_THAT(*evaluation.format_, Eq(format8_hi));
    EXPECT_THAT(evaluation.trigger_, Eq(Chunk::kTriggerInitial));
  }

  // With a playback rate of 16, we should choose format 16_hi.
  {
    playback_rate.set_rate(16);
    FormatEvaluation evaluation;
    evaluator.Evaluate(queue, playback_position, formats, &evaluation,
                       playback_rate);
    EXPECT_THAT(*evaluation.format_, Eq(format16_hi));
    EXPECT_THAT(evaluation.trigger_, Eq(Chunk::kTriggerInitial));
  }

  // With a playback rate of -16, we should choose format 16_hi.
  {
    playback_rate.set_rate(-16);
    FormatEvaluation evaluation;
    evaluator.Evaluate(queue, playback_position, formats, &evaluation,
                       playback_rate);
    EXPECT_THAT(*evaluation.format_, Eq(format16_hi));
    EXPECT_THAT(evaluation.trigger_, Eq(Chunk::kTriggerInitial));
  }

  VerifyAndClearMocks();
  EXPECT_CALL(meter_, GetBitrateEstimate())
      .WillRepeatedly(Return(format1_low.GetBitrate() - 1));

  // With a playback rate of 1, we should choose format 1_low (lowest, even
  // though estimate is lower)
  {
    playback_rate.set_rate(1);
    FormatEvaluation evaluation;
    evaluator.Evaluate(queue, playback_position, formats, &evaluation,
                       playback_rate);
    EXPECT_THAT(*evaluation.format_, Eq(format1_low));
    EXPECT_THAT(evaluation.trigger_, Eq(Chunk::kTriggerInitial));
  }

  // With a playback rate of 8, we should choose format 8_low.
  {
    playback_rate.set_rate(8);
    FormatEvaluation evaluation;
    evaluator.Evaluate(queue, playback_position, formats, &evaluation,
                       playback_rate);
    EXPECT_THAT(*evaluation.format_, Eq(format8_low));
    EXPECT_THAT(evaluation.trigger_, Eq(Chunk::kTriggerInitial));
  }

  // With a playback rate of -8, we should choose format 8_low.
  {
    playback_rate.set_rate(-8);
    FormatEvaluation evaluation;
    evaluator.Evaluate(queue, playback_position, formats, &evaluation,
                       playback_rate);
    EXPECT_THAT(*evaluation.format_, Eq(format8_low));
    EXPECT_THAT(evaluation.trigger_, Eq(Chunk::kTriggerInitial));
  }

  // With a playback rate of 16, we should choose format 16_low.
  {
    playback_rate.set_rate(16);
    FormatEvaluation evaluation;
    evaluator.Evaluate(queue, playback_position, formats, &evaluation,
                       playback_rate);
    EXPECT_THAT(*evaluation.format_, Eq(format16_low));
    EXPECT_THAT(evaluation.trigger_, Eq(Chunk::kTriggerInitial));
  }

  // With a playback rate of -16, we should choose format 16_low.
  {
    playback_rate.set_rate(-16);
    FormatEvaluation evaluation;
    evaluator.Evaluate(queue, playback_position, formats, &evaluation,
                       playback_rate);
    EXPECT_THAT(*evaluation.format_, Eq(format16_low));
    EXPECT_THAT(evaluation.trigger_, Eq(Chunk::kTriggerInitial));
  }

  VerifyAndClearMocks();
  EXPECT_CALL(meter_, GetBitrateEstimate())
      .WillRepeatedly(Return(format1_hi.GetBitrate() - 1));

  // With a playback rate of 1, we should choose format 1_mid (next highest
  // below format1_hi)
  {
    playback_rate.set_rate(1);
    FormatEvaluation evaluation;
    evaluator.Evaluate(queue, playback_position, formats, &evaluation,
                       playback_rate);
    EXPECT_THAT(*evaluation.format_, Eq(format1_mid));
    EXPECT_THAT(evaluation.trigger_, Eq(Chunk::kTriggerInitial));
  }

  // With a playback rate of 8, we should choose format 8_mid.
  {
    playback_rate.set_rate(8);
    FormatEvaluation evaluation;
    evaluator.Evaluate(queue, playback_position, formats, &evaluation,
                       playback_rate);
    EXPECT_THAT(*evaluation.format_, Eq(format8_mid));
    EXPECT_THAT(evaluation.trigger_, Eq(Chunk::kTriggerInitial));
  }

  // With a playback rate of -8, we should choose format 8_mid.
  {
    playback_rate.set_rate(-8);
    FormatEvaluation evaluation;
    evaluator.Evaluate(queue, playback_position, formats, &evaluation,
                       playback_rate);
    EXPECT_THAT(*evaluation.format_, Eq(format8_mid));
    EXPECT_THAT(evaluation.trigger_, Eq(Chunk::kTriggerInitial));
  }

  // With a playback rate of 12, we should choose format 16_mid.
  {
    playback_rate.set_rate(12);
    FormatEvaluation evaluation;
    evaluator.Evaluate(queue, playback_position, formats, &evaluation,
                       playback_rate);
    EXPECT_THAT(*evaluation.format_, Eq(format16_mid));
    EXPECT_THAT(evaluation.trigger_, Eq(Chunk::kTriggerInitial));
  }

  // With a playback rate of -12, we should choose format 16_mid.
  {
    playback_rate.set_rate(-12);
    FormatEvaluation evaluation;
    evaluator.Evaluate(queue, playback_position, formats, &evaluation,
                       playback_rate);
    EXPECT_THAT(*evaluation.format_, Eq(format16_mid));
    EXPECT_THAT(evaluation.trigger_, Eq(Chunk::kTriggerInitial));
  }

  // With a playback rate of 16, we should choose format 16_mid.
  {
    playback_rate.set_rate(16);
    FormatEvaluation evaluation;
    evaluator.Evaluate(queue, playback_position, formats, &evaluation,
                       playback_rate);
    EXPECT_THAT(*evaluation.format_, Eq(format16_mid));
    EXPECT_THAT(evaluation.trigger_, Eq(Chunk::kTriggerInitial));
  }

  // With a playback rate of -16, we should choose format 16_mid.
  {
    playback_rate.set_rate(-16);
    FormatEvaluation evaluation;
    evaluator.Evaluate(queue, playback_position, formats, &evaluation,
                       playback_rate);
    EXPECT_THAT(*evaluation.format_, Eq(format16_mid));
    EXPECT_THAT(evaluation.trigger_, Eq(Chunk::kTriggerInitial));
  }

  // With a playback rate of 18, we should still choose format 16_mid.
  {
    playback_rate.set_rate(18);
    FormatEvaluation evaluation;
    evaluator.Evaluate(queue, playback_position, formats, &evaluation,
                       playback_rate);
    EXPECT_THAT(*evaluation.format_, Eq(format16_mid));
    EXPECT_THAT(evaluation.trigger_, Eq(Chunk::kTriggerInitial));
  }

  // With a playback rate of -18, we should still choose format 16_mid.
  {
    playback_rate.set_rate(-18);
    FormatEvaluation evaluation;
    evaluator.Evaluate(queue, playback_position, formats, &evaluation,
                       playback_rate);
    EXPECT_THAT(*evaluation.format_, Eq(format16_mid));
    EXPECT_THAT(evaluation.trigger_, Eq(Chunk::kTriggerInitial));
  }

  evaluator.Disable();
}

}  // namespace chunk
}  // namespace ndash
