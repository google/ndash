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
#include "chunk/chunk.h"
#include "chunk/media_chunk_mock.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "playback_rate.h"
#include "upstream/data_spec.h"
#include "upstream/uri.h"

namespace ndash {
namespace chunk {

using ::testing::Eq;

TEST(DemoEvaluatorTest, TestEvaluateVideo) {
  upstream::DataSpec data_spec(upstream::Uri("dummy://"));
  PlaybackRate playback_rate;

  std::vector<util::Format> formats;
  util::Format format1("1", "video/mp4", 1280, 720, 20.0, 1, 2, 2345, 5000);
  util::Format format2("2", "video/mp4", 640, 480, 10.0, 1, 1, 1234, 10000);
  util::Format format3("3", "video/mp4", 1920, 1080, 30.0, 1, 3, 3456, 20000);

  std::deque<std::unique_ptr<MediaChunk>> queue1;
  for (int i = 0; i < 10; i++) {
    queue1.emplace_back(new ::testing::StrictMock<MockMediaChunk>(
        &data_spec, Chunk::kTriggerUnspecified, &format1, 0, 0, 0,
        Chunk::kNoParentId));
  }

  std::deque<std::unique_ptr<MediaChunk>> queue2;
  for (int i = 0; i < 20; i++) {
    queue2.emplace_back(new ::testing::StrictMock<MockMediaChunk>(
        &data_spec, Chunk::kTriggerUnspecified, &format2, 0, 0, 0,
        Chunk::kNoParentId));
  }

  std::deque<std::unique_ptr<MediaChunk>> queue3;
  for (int i = 0; i < 5; i++) {
    queue3.emplace_back(new ::testing::StrictMock<MockMediaChunk>(
        &data_spec, Chunk::kTriggerUnspecified, &format3, 0, 0, 0,
        Chunk::kNoParentId));
  }

  base::TimeDelta playback_position1(base::TimeDelta::FromSeconds(10));
  base::TimeDelta playback_position2(base::TimeDelta::FromSeconds(20));
  base::TimeDelta playback_position3(base::TimeDelta::FromSeconds(30));

  formats.push_back(format1);
  formats.push_back(format2);

  util::Format* first_format = &formats.at(0);
  EXPECT_THAT(*first_format, Eq(format1));

  DemoEvaluator evaluator;
  evaluator.Enable();

  // With just the first two formats pushed, the demo evaluator should chose the
  // second format as it has the higher bitrate.
  {
    FormatEvaluation evaluation;
    evaluator.Evaluate(queue1, playback_position1, formats, &evaluation,
                       playback_rate);
    EXPECT_THAT(evaluation.queue_size_, Eq(0));
    EXPECT_THAT(*evaluation.format_, Eq(formats[1]));
    EXPECT_THAT(evaluation.trigger_, Eq(Chunk::kTriggerInitial));
  }

  formats.push_back(format3);

  // After the third format is added, the evaluator should switch to that since
  // it has higher bitrate.
  {
    FormatEvaluation evaluation;
    evaluator.Evaluate(queue2, playback_position2, formats, &evaluation,
                       playback_rate);
    EXPECT_THAT(evaluation.queue_size_, Eq(0));
    EXPECT_THAT(*evaluation.format_, Eq(formats[2]));
    EXPECT_THAT(evaluation.trigger_, Eq(Chunk::kTriggerInitial));
  }

  evaluator.Disable();
}

TEST(DemoEvaluatorTest, TestEvaluateAudio) {
  upstream::DataSpec data_spec(upstream::Uri("dummy://"));
  PlaybackRate playback_rate;

  std::vector<util::Format> formats;
  util::Format format1("1", "audio/ac3", -1, -1, 20.0, 1, 2, 2345, 10000);
  util::Format format2("2", "audio/ac3", -1, -1, 10.0, 1, 1, 1234, 15000);
  util::Format format3("3", "audio/ac3", -1, -1, 30.0, 1, 3, 3456, 5000);

  std::deque<std::unique_ptr<MediaChunk>> queue1;
  for (int i = 0; i < 10; i++) {
    queue1.emplace_back(new ::testing::StrictMock<MockMediaChunk>(
        &data_spec, Chunk::kTriggerUnspecified, &format1, 0, 0, 0,
        Chunk::kNoParentId));
  }

  std::deque<std::unique_ptr<MediaChunk>> queue2;
  for (int i = 0; i < 20; i++) {
    queue2.emplace_back(new ::testing::StrictMock<MockMediaChunk>(
        &data_spec, Chunk::kTriggerUnspecified, &format2, 0, 0, 0,
        Chunk::kNoParentId));
  }

  std::deque<std::unique_ptr<MediaChunk>> queue3;
  for (int i = 0; i < 5; i++) {
    queue3.emplace_back(new ::testing::StrictMock<MockMediaChunk>(
        &data_spec, Chunk::kTriggerUnspecified, &format3, 0, 0, 0,
        Chunk::kNoParentId));
  }

  base::TimeDelta playback_position1(base::TimeDelta::FromSeconds(10));
  base::TimeDelta playback_position2(base::TimeDelta::FromSeconds(20));
  base::TimeDelta playback_position3(base::TimeDelta::FromSeconds(30));

  formats.push_back(format1);
  formats.push_back(format2);

  util::Format* first_format = &formats.at(0);
  EXPECT_THAT(*first_format, Eq(format1));

  DemoEvaluator evaluator;
  evaluator.Enable();

  // With just the first two formats pushed, the demo evaluator should chose the
  // second format as it has the larger bitrate.
  {
    FormatEvaluation evaluation;
    evaluator.Evaluate(queue1, playback_position1, formats, &evaluation,
                       playback_rate);
    EXPECT_THAT(evaluation.queue_size_, Eq(0));
    EXPECT_THAT(*evaluation.format_, Eq(formats[1]));
    EXPECT_THAT(evaluation.trigger_, Eq(Chunk::kTriggerInitial));
  }

  formats.push_back(format3);

  // After pushing the third format, the evaluator should stick with the second
  // format as it still has the highest bitrate.
  {
    FormatEvaluation evaluation;
    evaluator.Evaluate(queue2, playback_position2, formats, &evaluation,
                       playback_rate);
    EXPECT_THAT(evaluation.queue_size_, Eq(0));
    EXPECT_THAT(*evaluation.format_, Eq(formats[1]));
    EXPECT_THAT(evaluation.trigger_, Eq(Chunk::kTriggerInitial));
  }

  evaluator.Disable();
}

TEST(DemoEvaluatorTest, TestEvaluateVideoMaxPlayoutRate) {
  upstream::DataSpec data_spec(upstream::Uri("dummy://"));
  PlaybackRate playback_rate;

  std::vector<util::Format> formats;
  // MaxPlayoutRate 1
  util::Format format1("1", "video/mp4", 1280, 720, 20.0, 1, 2, 2345, 5000);
  util::Format format2("2", "video/mp4", 640, 480, 10.0, 1, 1, 1234, 10000);
  util::Format format3("3", "video/mp4", 1920, 1080, 30.0, 1, 3, 3456, 20000);

  // MaxPlayoutRate 8
  util::Format format4("4", "video/mp4", 1280, 720, 20.0, 8, 2, 2345, 5000);
  util::Format format5("5", "video/mp4", 640, 480, 10.0, 8, 1, 1234, 10000);
  util::Format format6("6", "video/mp4", 1920, 1080, 30.0, 8, 3, 3456, 20000);

  // MaxPlayoutRate 16
  util::Format format7("7", "video/mp4", 1280, 720, 20.0, 16, 2, 2345, 5000);
  util::Format format8("8", "video/mp4", 640, 480, 10.0, 16, 1, 1234, 10000);
  util::Format format9("9", "video/mp4", 1920, 1080, 30.0, 16, 3, 3456, 20000);

  formats.push_back(format1);
  formats.push_back(format2);
  formats.push_back(format3);
  formats.push_back(format4);
  formats.push_back(format5);
  formats.push_back(format6);
  formats.push_back(format7);
  formats.push_back(format8);
  formats.push_back(format9);

  util::Format* first_format = &formats.at(0);
  EXPECT_THAT(*first_format, Eq(format1));

  std::deque<std::unique_ptr<MediaChunk>> queue;
  base::TimeDelta playback_position(base::TimeDelta::FromSeconds(0));

  DemoEvaluator evaluator;
  evaluator.Enable();

  // With a playback rate of 1, we should chose format 3.
  {
    playback_rate.set_rate(1);
    FormatEvaluation evaluation;
    evaluator.Evaluate(queue, playback_position, formats, &evaluation,
                       playback_rate);
    EXPECT_THAT(*evaluation.format_, Eq(formats[2]));
    EXPECT_THAT(evaluation.trigger_, Eq(Chunk::kTriggerInitial));
  }

  // With a playback rate of 8, we should chose format 6.
  {
    playback_rate.set_rate(8);
    FormatEvaluation evaluation;
    evaluator.Evaluate(queue, playback_position, formats, &evaluation,
                       playback_rate);
    EXPECT_THAT(*evaluation.format_, Eq(formats[5]));
    EXPECT_THAT(evaluation.trigger_, Eq(Chunk::kTriggerInitial));
  }

  // With a playback rate of 16, we should chose format 9.
  {
    playback_rate.set_rate(16);
    FormatEvaluation evaluation;
    evaluator.Evaluate(queue, playback_position, formats, &evaluation,
                       playback_rate);
    EXPECT_THAT(*evaluation.format_, Eq(formats[8]));
    EXPECT_THAT(evaluation.trigger_, Eq(Chunk::kTriggerInitial));
  }

  evaluator.Disable();
}

}  // namespace chunk
}  // namespace ndash
