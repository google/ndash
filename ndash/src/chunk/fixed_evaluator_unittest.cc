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

#include "chunk/fixed_evaluator.h"
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

TEST(FixedEvaluatorTest, TestEvaluate) {
  upstream::DataSpec data_spec(upstream::Uri("dummy://"));
  PlaybackRate playback_rate;

  std::vector<util::Format> formats;
  util::Format format1("1", "text/plain", 10, 10, 10.0, 1, 1, 1234, 9999);
  util::Format format2("2", "text/plain", 20, 20, 20.0, 1, 2, 2345, 8888);
  util::Format format3("3", "text/plain", 30, 30, 30.0, 1, 3, 3456, 7777);

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
  formats.push_back(format3);

  util::Format* first_format = &formats.at(0);
  EXPECT_THAT(*first_format, Eq(format1));

  FixedEvaluator evaluator;
  evaluator.Enable();

  {
    FormatEvaluation evaluation;
    evaluator.Evaluate(queue1, playback_position1, formats, &evaluation,
                       playback_rate);
    EXPECT_THAT(evaluation.queue_size_, Eq(0));
    EXPECT_THAT(*evaluation.format_, Eq(*first_format));
    EXPECT_THAT(evaluation.trigger_, Eq(Chunk::kTriggerInitial));
  }

  {
    FormatEvaluation evaluation;
    evaluator.Evaluate(queue2, playback_position2, formats, &evaluation,
                       playback_rate);
    EXPECT_THAT(evaluation.queue_size_, Eq(0));
    EXPECT_THAT(*evaluation.format_, Eq(*first_format));
    EXPECT_THAT(evaluation.trigger_, Eq(Chunk::kTriggerInitial));
  }

  {
    FormatEvaluation evaluation;
    evaluator.Evaluate(queue3, playback_position3, formats, &evaluation,
                       playback_rate);
    EXPECT_THAT(evaluation.queue_size_, Eq(0));
    EXPECT_THAT(*evaluation.format_, Eq(*first_format));
    EXPECT_THAT(evaluation.trigger_, Eq(Chunk::kTriggerInitial));

    evaluator.Disable();
  }
}

}  // namespace chunk
}  // namespace ndash
