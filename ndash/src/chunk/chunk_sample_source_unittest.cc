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

#include "chunk/chunk_sample_source.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "chunk/base_media_chunk_mock.h"
#include "chunk/chunk_source_mock.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "load_control.h"
#include "media_format.h"
#include "playback_rate.h"
#include "track_criteria.h"
#include "upstream/allocator_mock.h"
#include "upstream/data_spec.h"
#include "upstream/loader_mock.h"
#include "upstream/uri.h"

namespace ndash {
namespace chunk {

namespace {
void SourceReaderDisabled(SampleSourceReaderInterface* source,
                          base::WaitableEvent* waitable_event) {
  waitable_event->Signal();
}

void DisableSourceReader(chunk::ChunkSampleSource* css,
                         base::WaitableEvent* waitable_event) {
  base::Closure disable_done_callback =
      base::Bind(&SourceReaderDisabled, base::Unretained(css),
                 base::Unretained(waitable_event));
  css->Disable(&disable_done_callback);
}

// Makes sure the callback is invoked on the current thread's task runner.
// Unfortunately, this means we need a thread.
void AsyncDisableHelper(chunk::ChunkSampleSource* css) {
  base::WaitableEvent waiter(false, false);
  base::Thread t("ChunkSampleSource Disable Helper");
  t.Start();
  t.task_runner()->PostTask(
      FROM_HERE, base::Bind(&DisableSourceReader, base::Unretained(css),
                            base::Unretained(&waiter)));
  waiter.Wait();
}
}  // namespace

using ::testing::_;
using ::testing::Eq;
using ::testing::IsNull;
using ::testing::Ref;
using ::testing::Return;

TEST(ChunkSampleSource, Constructor) {
  MockChunkSource chunk_source;
  upstream::MockAllocator allocator;
  LoadControl load_control(&allocator);
  PlaybackRate playback_rate;

  EXPECT_CALL(allocator, GetIndividualAllocationLength())
      .WillRepeatedly(Return(1024));

  std::unique_ptr<MediaFormat> media_format =
      MediaFormat::CreateVideoFormat("1", "video/mp4", "h264", 2200000, 32768,
                                     1234567, 640, 480, nullptr, 16, 45, 1.666);
  EXPECT_CALL(chunk_source, GetDurationUs())
      .WillRepeatedly(Return(media_format->GetDurationUs()));

  ChunkSampleSource css(&chunk_source, &load_control, &playback_rate, 10240);
}

TEST(ChunkSampleSource, EnableUseDisable) {
  // This doesn't test every code path but it does exercise a
  // decent 'chunk' of them (ha!).
  MockChunkSource chunk_source;
  upstream::MockAllocator allocator;
  LoadControl load_control(&allocator);
  PlaybackRate playback_rate;

  EXPECT_CALL(allocator, GetIndividualAllocationLength())
      .WillRepeatedly(Return(1024));
  EXPECT_CALL(chunk_source, Prepare()).WillOnce(Return(true));

  std::unique_ptr<MediaFormat> media_format =
      MediaFormat::CreateVideoFormat("1", "video/mp4", "h264", 2200000, 32768,
                                     1234567, 640, 480, nullptr, 16, 45, 1.666);
  EXPECT_CALL(chunk_source, GetDurationUs())
      .WillRepeatedly(Return(media_format->GetDurationUs()));

  upstream::Uri ds_uri("http://somewhere");
  upstream::DataSpec spec(ds_uri);
  util::Format f("id1", "video/mpeg4", 320, 480, 29.98, 1, 2, 48000, 6000000,
                 "en_us", "codec");

  class MyLoaderFactory : public LoaderFactoryInterface {
   public:
    MyLoaderFactory() { loader = new upstream::MockLoaderInterface; }
    ~MyLoaderFactory() override {}
    std::unique_ptr<upstream::LoaderInterface> CreateLoader(
        ChunkSourceInterface* chunk_source) override {
      return std::unique_ptr<upstream::LoaderInterface>(loader);
    }
    upstream::MockLoaderInterface* loader;
  };

  std::unique_ptr<MyLoaderFactory> loader_factory(new MyLoaderFactory);
  upstream::MockLoaderInterface* loader = loader_factory->loader;
  ChunkSampleSource css(&chunk_source, &load_control, &playback_rate, 10240,
                        nullptr, 0, kDefaultMinLoadableRetryCount,
                        std::move(loader_factory));

  css.Register();
  EXPECT_EQ(true, css.Prepare(0));
  EXPECT_TRUE(css.GetDurationUs() == 1234567);

  EXPECT_CALL(chunk_source, Enable(_)).Times(1);
  EXPECT_CALL(allocator, GetTotalBytesAllocated()).WillRepeatedly(Return(0));
  EXPECT_CALL(*loader, IsLoading()).WillRepeatedly(Return(false));

  TrackCriteria criteria("video");
  css.Enable(&criteria, 0);

  base::TimeDelta t;
  EXPECT_CALL(chunk_source, ContinueBuffering(t)).Times(1);
  css.ContinueBuffering(0);
  //
  EXPECT_CALL(chunk_source, CanContinueBuffering()).WillOnce(Return(false));
  EXPECT_FALSE(css.CanContinueBuffering());

  EXPECT_EQ(0, css.GetBufferedPositionUs());

  EXPECT_EQ(SampleSourceReaderInterface::ReadResult::NO_DISCONTINUITY,
            css.ReadDiscontinuity());

  MediaFormatHolder format;
  SampleHolder sample(true);
  EXPECT_EQ(SampleSourceReaderInterface::ReadResult::NOTHING_READ,
            css.ReadData(0, &format, &sample));

  // Now give a media chunk to the mock source
  MockBaseMediaChunk* mock_chunk = new MockBaseMediaChunk(
      &spec, Chunk::kTriggerAdaptive, &f, 0, 1000, 0, true, Chunk::kNoParentId);
  std::unique_ptr<MediaChunk> chunk(mock_chunk);
  chunk_source.SetMediaChunk(std::move(chunk));

  // Now it should actually buffer something
  EXPECT_CALL(chunk_source, CanContinueBuffering())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(chunk_source, ContinueBuffering(t)).Times(1);
  EXPECT_TRUE(css.CanContinueBuffering());
  EXPECT_CALL(*loader, StartLoading(_, _)).Times(1);

  css.ContinueBuffering(0);

  // Fake the load complete callback.
  EXPECT_CALL(chunk_source, OnChunkLoadCompleted(_)).Times(1);
  EXPECT_CALL(*mock_chunk, GetNumBytesLoaded()).WillOnce(Return(1024));

  css.LoadComplete(chunk.get(), upstream::LOAD_COMPLETE);

  EXPECT_CALL(chunk_source, Disable(_)).Times(1);
  EXPECT_CALL(allocator, Trim(_)).Times(1);

  AsyncDisableHelper(&css);

  // Fake loading in progress so release will cancel it.
  EXPECT_CALL(*loader, IsLoading()).WillRepeatedly(Return(true));
  EXPECT_CALL(*loader, CancelLoading()).Times(1);

  css.Release();
}

}  // namespace chunk
}  // namespace ndash
