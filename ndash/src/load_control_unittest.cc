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

#include "load_control.h"

#include "gtest/gtest.h"
#include "upstream/allocator_mock.h"
#include "upstream/default_allocator.h"
#include "upstream/loader_mock.h"
#include "util/util.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::Le;
using ::testing::Return;

namespace ndash {

TEST(LoadControlTests, LoadControl) {
  upstream::MockAllocator allocator;

  EXPECT_CALL(allocator, Allocate()).WillRepeatedly(Return(nullptr));

  double low_time_watermark = 15000;
  double high_time_watermark = 30000;
  double low_buf_watermark = 0.2;
  double high_buf_watermark = 0.8;
  LoadControl load_control(&allocator, nullptr, low_time_watermark,
                           high_time_watermark, low_buf_watermark,
                           high_buf_watermark);

  upstream::MockLoaderInterface video_mock_loader;
  upstream::MockLoaderInterface audio_mock_loader;
  load_control.Register(&video_mock_loader, 1024 * 90);
  load_control.Register(&audio_mock_loader, 1024 * 10);

  int64_t playback_pos_us = 0;
  int64_t video_load_pos_us = 0;
  int64_t audio_load_pos_us = 0;

  int video_allocation_count = 0;
  int audio_allocation_count = 0;

  bool next_loader;
  bool loading_video = false;
  bool loading_audio = false;

  int video_loaded_when = -1;
  int audio_loaded_when = -1;

  // The following loop simulates the typical calls made to a load control
  // for 80 seconds of play of 1 video and 1 audio track.  Video segments take
  // 50 ms to download while audio segments take 30 ms.  The allocator's buffer
  // is quickly filled within the first few iterations.  The loader should
  // prevent the buffer from going beyond (kDefaultHighBufferLoad * 100) % of
  // the total. Also, no loader should get ahead of the other.
  int iteration = 0;
  while (playback_pos_us < util::kMicrosPerSecond * 80) {
    // Simulate memory being allocated as we load audio/video data.
    EXPECT_CALL(allocator, GetTotalBytesAllocated())
        .WillRepeatedly(Return(video_allocation_count * 1024 +
                               audio_allocation_count * 1024));

    if (iteration >= video_loaded_when) {
      loading_video = false;
      video_loaded_when = -1;
    }

    EXPECT_CALL(video_mock_loader, IsLoading())
        .WillRepeatedly(Return(loading_video));

    next_loader = load_control.Update(&video_mock_loader, playback_pos_us,
                                      video_load_pos_us, loading_video);
    if (next_loader && !loading_video) {
      allocator.Allocate();
      video_allocation_count++;
      video_load_pos_us += util::kMicrosPerSecond * 2.5;
      loading_video = true;
      // Simulate video segment loaded in 50ms
      video_loaded_when = iteration + 5;
    }

    if (iteration >= audio_loaded_when) {
      loading_audio = false;
      audio_loaded_when = -1;
    }

    EXPECT_CALL(audio_mock_loader, IsLoading())
        .WillRepeatedly(Return(loading_audio));

    next_loader = load_control.Update(&audio_mock_loader, playback_pos_us,
                                      audio_load_pos_us, loading_audio);
    if (next_loader && !loading_audio) {
      // Every 10 audio loads fills 1 allocation block in our simulation.
      if (audio_allocation_count % 10 == 0) {
        allocator.Allocate();
      }
      audio_allocation_count++;
      audio_load_pos_us += util::kMicrosPerSecond * 2.5;
      loading_audio = true;
      // Simulate video segment loaded in 30ms
      audio_loaded_when = iteration + 3;
    }

    // Advance simulated play back position by 1/10th second on each iteration.
    // Don't allow it to go past what we've actually 'loaded' since that's
    // not possible.
    int64_t min_load_pos = std::min(video_load_pos_us, audio_load_pos_us);
    playback_pos_us =
        std::min(min_load_pos, playback_pos_us + util::kMicrosPerSecond / 10);

    // Load control should not allow us to go above high water mark.
    EXPECT_THAT(allocator.GetTotalBytesAllocated(),
                Le(1024 * 100 * high_buf_watermark));

    // The two loaders should not be more than 2.5 seconds apart from each
    // other.
    // TODO(rmrossi): cdrolle@ suggested we remove the high/low time watermark
    // logic since it never seems to apply to our loaders.
    EXPECT_THAT(std::abs(audio_load_pos_us - video_load_pos_us),
                Le(util::kMicrosPerSecond * 2.5));

    iteration++;
  }

  // TODO(rmrossi): We should also test that draining the buffer
  // (via Allocator::Release()) resumes loading.
}

TEST(LoadControlTests, LoadControlListener) {
  class MyListener : public LoadControlEventListenerInterface {
   public:
    MyListener(bool* last_loading_state) {
      last_loading_state_ = last_loading_state;
    }
    void OnLoadingChanged(bool loading) override {
      *last_loading_state_ = loading;
    }

   private:
    bool* last_loading_state_;
  };

  bool loading;
  MyListener my_listener(&loading);
  upstream::MockAllocator allocator;

  EXPECT_CALL(allocator, Allocate()).WillRepeatedly(Return(nullptr));

  LoadControl load_control(&allocator, &my_listener);

  upstream::MockLoaderInterface mock_loader;
  load_control.Register(&mock_loader, 1024 * 100);

  EXPECT_CALL(allocator, GetTotalBytesAllocated()).WillRepeatedly(Return(1024));
  load_control.Update(&mock_loader, 0, 0, true);
  EXPECT_THAT(loading, Eq(true));

  EXPECT_CALL(allocator, GetTotalBytesAllocated())
      .WillRepeatedly(Return(1024 * 100));
  load_control.Update(&mock_loader, 0, -1, false);
  EXPECT_THAT(loading, Eq(false));
}

}  // namespace ndash
