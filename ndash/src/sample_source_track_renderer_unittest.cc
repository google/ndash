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

#include "sample_source_track_renderer.h"

#include "base/bind.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "sample_source_mock.h"
#include "sample_source_reader_mock.h"

namespace ndash {

namespace {
std::unique_ptr<char[]> MakeInitData() {
  std::unique_ptr<char[]> init_data(new char[16]);
  for (int i = 0; i < 16; i++) {
    init_data[i] = i;
  }
  return init_data;
}

class ClosureFunctor {
 public:
  explicit ClosureFunctor(const base::Closure& c) : c_(c) {}
  ClosureFunctor(const ClosureFunctor& f) : c_(f.c_) {}
  ~ClosureFunctor() {}
  ClosureFunctor& operator=(const ClosureFunctor& f) {
    c_ = f.c_;
    return *this;
  }
  void operator()() { c_.Run(); }

 private:
  base::Closure c_;
};

void TrackDisabled(TrackRenderer* track, bool* is_disabled) {
  *is_disabled = true;
}
}  // namespace

using ::testing::_;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::AtLeast;

TEST(SampleSourceTrackRenderer, StateTransitions) {
  std::unique_ptr<MediaFormat> format = MediaFormat::CreateVideoFormat(
      "1", "video/mp4", "h264", 2200000, 32768, 1234567, 640, 480,
      MakeInitData(), 16, 45, 1.666);

  SampleSourceReaderMock source_reader;
  SampleSourceMock source;
  EXPECT_CALL(source, Register()).WillOnce(Return(&source_reader));
  EXPECT_CALL(source_reader, Prepare(0)).WillOnce(Return(true));
  EXPECT_CALL(source_reader, GetDurationUs())
      .WillRepeatedly(Return(format->GetDurationUs()));

  SampleSourceTrackRenderer track(&source);

  EXPECT_EQ(true, track.Prepare(0));
  EXPECT_EQ(TrackRenderer::PREPARED, track.GetState());

  for (int enable_count = 0; enable_count < 1; enable_count++) {
    EXPECT_CALL(source_reader, Enable(_, 0));

    TrackCriteria criteria("video");
    EXPECT_EQ(true, track.Enable(&criteria, 0, false));
    EXPECT_EQ(TrackRenderer::ENABLED, track.GetState());

    for (int start_stop_count = 0; start_stop_count < 2; start_stop_count++) {
      EXPECT_EQ(true, track.Start());
      EXPECT_EQ(TrackRenderer::STARTED, track.GetState());

      EXPECT_EQ(true, track.Stop());
      EXPECT_EQ(TrackRenderer::ENABLED, track.GetState());
    }

    base::Closure invoke_done_callback =
        base::Bind(&TrackRenderer::DisableDone, base::Unretained(&track),
                   base::Unretained(&source_reader));
    EXPECT_CALL(source_reader, Disable(_))
        .WillOnce(InvokeWithoutArgs(ClosureFunctor(invoke_done_callback)));

    bool was_disabled = false;
    base::Closure disable_done_callback =
        base::Bind(&TrackDisabled, base::Unretained(&track),
                   base::Unretained(&was_disabled));
    EXPECT_EQ(true, track.Disable(&disable_done_callback));
    EXPECT_EQ(TrackRenderer::PREPARED, track.GetState());

    // Make sure callback was invoked.
    EXPECT_TRUE(was_disabled);
  }

  EXPECT_CALL(source_reader, Release());

  EXPECT_EQ(true, track.Release());
  EXPECT_EQ(TrackRenderer::RELEASED, track.GetState());

  // One more call to release expected from the destructor.
  EXPECT_CALL(source_reader, Release());
}

TEST(SampleSourceTrackRenderer, BufferAndReadFrames) {
  std::unique_ptr<MediaFormat> format = MediaFormat::CreateVideoFormat(
      "1", "video/mp4", "h264", 2200000, 32768, 1234567, 640, 480,
      MakeInitData(), 16, 45, 1.666);

  SampleSourceReaderMock source_reader;
  SampleSourceMock source;
  EXPECT_CALL(source, Register()).WillOnce(Return(&source_reader));
  EXPECT_CALL(source_reader, Prepare(0)).WillOnce(Return(true));
  EXPECT_CALL(source_reader, GetDurationUs())
      .WillRepeatedly(Return(format->GetDurationUs()));

  SampleSourceTrackRenderer track(&source);

  EXPECT_EQ(true, track.Prepare(0));
  EXPECT_EQ(TrackRenderer::PREPARED, track.GetState());
  TrackCriteria criteria("video");
  EXPECT_CALL(source_reader, Enable(_, 0));
  EXPECT_EQ(true, track.Enable(&criteria, 0, false));
  EXPECT_EQ(TrackRenderer::ENABLED, track.GetState());
  EXPECT_EQ(true, track.Start());
  EXPECT_EQ(TrackRenderer::STARTED, track.GetState());

  MediaFormatHolder format_holder;
  SampleHolder sample_holder(true);
  bool error;

  // Nothing to read yet.
  EXPECT_CALL(source_reader, ReadDiscontinuity())
      .WillOnce(Return(SampleSourceReaderInterface::NO_DISCONTINUITY));
  int result = track.ReadFrame(0, &format_holder, &sample_holder, &error);
  EXPECT_EQ(SampleSourceReaderInterface::NOTHING_READ, result);

  // Buffer some data.
  EXPECT_CALL(source_reader, ContinueBuffering(0)).WillRepeatedly(Return(true));
  track.Buffer(0);

  // Something to read.
  EXPECT_CALL(source_reader, ReadDiscontinuity())
      .WillOnce(Return(SampleSourceReaderInterface::NO_DISCONTINUITY));
  EXPECT_CALL(source_reader, ReadData(0, _, _))
      .WillOnce(Return(SampleSourceReaderInterface::SAMPLE_READ));
  result = track.ReadFrame(0, &format_holder, &sample_holder, &error);
  EXPECT_EQ(SampleSourceReaderInterface::SAMPLE_READ, result);

  EXPECT_CALL(source_reader, Release());
}

}  // namespace ndash
