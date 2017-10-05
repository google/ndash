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

#include <cstdint>
#include <memory>

#include "base/time/time.h"
#include "chunk/chunk_extractor_wrapper.h"
#include "chunk/container_media_chunk.h"
#include "drm/drm_init_data_mock.h"
#include "extractor/extractor.h"
#include "extractor/extractor_input_mock.h"
#include "extractor/extractor_mock.h"
#include "extractor/indexed_track_output_mock.h"
#include "extractor/seek_map.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "media_format.h"
#include "test_matchers.h"
#include "upstream/data_source_mock.h"
#include "upstream/data_spec.h"
#include "upstream/uri.h"
#include "util/format.h"

namespace ndash {
namespace chunk {

using ::testing::ByRef;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::IsNull;
using ::testing::Mock;
using ::testing::Ne;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::Sequence;
using ::testing::_;

TEST(ContainerMediaChunkTest, Accessors) {
  upstream::Uri dummy_uri("dummy://");
  upstream::DataSpec data_spec(dummy_uri);
  util::Format format("", "", 0, 0, 0.0, 1, 0, 0, 0);
  constexpr int64_t kTestStartTime = 1;
  constexpr int64_t kTestEndTime = 2;
  constexpr int32_t kTestChunkIndex = 3;
  const base::TimeDelta kSampleOffsetBase =
      base::TimeDelta::FromMicroseconds(5);
  const base::TimeDelta kSampleOffsetFormat =
      base::TimeDelta::FromMicroseconds(80);
  const base::TimeDelta kSampleOffsetTotal =
      kSampleOffsetBase + kSampleOffsetFormat;
  std::unique_ptr<const MediaFormat> media_format(
      MediaFormat::CreateVideoFormat("1", "video/mp4", "h264", 2200000, 32768,
                                     1234567, 640, 480, nullptr, 16, 45,
                                     1.666));
  const MediaFormat* media_format_ptr = media_format.get();
  const std::unique_ptr<MediaFormat> media_format_so =
      media_format_ptr->CopyWithSubsampleOffsetUs(
          kSampleOffsetFormat.InMicroseconds());
  const std::unique_ptr<MediaFormat> media_format_so_updated =
      media_format_ptr->CopyWithSubsampleOffsetUs(
          kSampleOffsetTotal.InMicroseconds());

  drm::MockDrmInitData* drm_init_data = new drm::MockDrmInitData;
  scoped_refptr<drm::RefCountedDrmInitData> drm_init_data_ref(drm_init_data);
  upstream::MockDataSource data_source;
  extractor::MockExtractor* mock_extractor = new extractor::MockExtractor;
  std::unique_ptr<extractor::ExtractorInterface> extractor(mock_extractor);
  scoped_refptr<ChunkExtractorWrapper> extractor_wrapper(
      new ChunkExtractorWrapper(std::move(extractor)));

  ContainerMediaChunk cmc(&data_source, &data_spec, Chunk::kTriggerUnspecified,
                          &format, kTestStartTime, kTestEndTime,
                          kTestChunkIndex, kSampleOffsetBase, extractor_wrapper,
                          std::move(media_format), drm_init_data, true,
                          Chunk::kNoParentId);

  EXPECT_THAT(cmc.GetMediaFormat(), media_format_ptr);
  EXPECT_THAT(cmc.GetDrmInitData(), Eq(drm_init_data));
  EXPECT_THAT(cmc.GetNumBytesLoaded(), Eq(0));
  EXPECT_THAT(cmc.IsLoadCanceled(), Eq(false));

  cmc.SetDrmInitData(nullptr);
  EXPECT_THAT(cmc.GetDrmInitData(), IsNull());

  cmc.GiveFormat(nullptr);
  EXPECT_THAT(cmc.GetMediaFormat(), IsNull());

  cmc.GiveFormat(
      std::unique_ptr<const MediaFormat>(new MediaFormat(*media_format_so)));
  EXPECT_THAT(cmc.GetMediaFormat(),
              Pointee(Eq(ByRef(*media_format_so_updated))));
}

TEST(ContainerMediaChunkTest, TrackOutputPassthru) {
  upstream::Uri dummy_uri("dummy://");
  upstream::DataSpec data_spec(dummy_uri);
  util::Format format("", "", 0, 0, 0.0, 1, 0, 0, 0);
  constexpr int64_t kTestStartTime = 1;
  constexpr int64_t kTestEndTime = 2;
  constexpr int32_t kTestChunkIndex = 3;

  extractor::MockExtractor* mock_extractor = new extractor::MockExtractor;
  std::unique_ptr<extractor::ExtractorInterface> extractor(mock_extractor);
  scoped_refptr<ChunkExtractorWrapper> extractor_wrapper(
      new ChunkExtractorWrapper(std::move(extractor)));

  ContainerMediaChunk cmc(nullptr, &data_spec, Chunk::kTriggerUnspecified,
                          &format, kTestStartTime, kTestEndTime,
                          kTestChunkIndex, base::TimeDelta(), extractor_wrapper,
                          nullptr, nullptr, true, Chunk::kNoParentId);

  extractor::MockExtractorInput extractor_input;
  constexpr size_t kMaxLength = 678;
  int64_t bytes_appended = 0;

  extractor::MockIndexedTrackOutput output;

  EXPECT_CALL(output, GetWriteIndex()).WillOnce(Return(0));
  cmc.Init(&output);
  Mock::VerifyAndClearExpectations(&output);

  EXPECT_CALL(output, WriteSampleData(&extractor_input, kMaxLength, true,
                                      &bytes_appended));
  cmc.WriteSampleData(&extractor_input, kMaxLength, true, &bytes_appended);
  Mock::VerifyAndClearExpectations(&output);

  char data[16];

  EXPECT_CALL(output, WriteSampleData(data, sizeof(data)));
  cmc.WriteSampleData(data, sizeof(data));
  Mock::VerifyAndClearExpectations(&output);

  EXPECT_CALL(output, WriteSampleDataFixThis(data, sizeof(data), true,
                                             &bytes_appended));
  cmc.WriteSampleDataFixThis(data, sizeof(data), true, &bytes_appended);
  Mock::VerifyAndClearExpectations(&output);

  constexpr int64_t kTestDuration = kTestEndTime - kTestStartTime;
  constexpr int32_t kFlags = 3456;
  EXPECT_CALL(output,
              WriteSampleMetadata(kTestStartTime, kTestDuration, kFlags,
                                  kMaxLength, 0, nullptr, nullptr, 0, 0));
  cmc.WriteSampleMetadata(kTestStartTime, kTestDuration, kFlags, kMaxLength, 0,
                          nullptr, nullptr, 0, 0);
  Mock::VerifyAndClearExpectations(&output);

  cmc.GiveSeekMap(nullptr);
}

TEST(ContainerMediaChunkTest, TestLoadSuccess) {
  upstream::Uri dummy_uri("dummy://");
  upstream::DataSpec data_spec(dummy_uri);
  util::Format format("", "", 0, 0, 0.0, 1, 0, 0, 0);
  constexpr int64_t kTestStartTime = 1;
  constexpr int64_t kTestEndTime = 2;
  constexpr int32_t kTestChunkIndex = 3;
  const base::TimeDelta kSampleOffset = base::TimeDelta::FromMicroseconds(5000);
  std::unique_ptr<MediaFormat> media_format =
      MediaFormat::CreateVideoFormat("1", "video/mp4", "h264", 2200000, 32768,
                                     1234567, 640, 480, nullptr, 16, 45, 1.666);
  constexpr ssize_t kReadSize = 888;

  upstream::MockDataSource data_source;
  extractor::MockExtractor* mock_extractor = new extractor::MockExtractor;
  std::unique_ptr<extractor::ExtractorInterface> extractor(mock_extractor);
  scoped_refptr<ChunkExtractorWrapper> extractor_wrapper(
      new ChunkExtractorWrapper(std::move(extractor)));

  ContainerMediaChunk cmc(&data_source, &data_spec, Chunk::kTriggerUnspecified,
                          &format, kTestStartTime, kTestEndTime,
                          kTestChunkIndex, kSampleOffset, extractor_wrapper,
                          std::move(media_format), nullptr, true,
                          Chunk::kNoParentId);

  Sequence seq;
  extractor::ExtractorInputInterface* extractor_input = nullptr;

  EXPECT_CALL(data_source, Open(DebugStringEquals(data_spec.DebugString()), _))
      .InSequence(seq)
      .WillOnce(Return(0));
  EXPECT_CALL(*mock_extractor, Init(extractor_wrapper.get()))
      .InSequence(seq)
      .WillOnce(Return());
  EXPECT_CALL(*mock_extractor, Read(_, IsNull()))
      .InSequence(seq)
      .WillOnce(DoAll(SaveArg<0>(&extractor_input),
                      Return(extractor::ExtractorInterface::RESULT_CONTINUE)));
  EXPECT_CALL(*mock_extractor, Read(Eq(ByRef(extractor_input)), IsNull()))
      .InSequence(seq)
      .WillOnce(Invoke([](extractor::ExtractorInputInterface* input,
                          int64_t* seek_position) {
        input->Read(nullptr, 0);
        return extractor::ExtractorInterface::RESULT_CONTINUE;
      }));
  EXPECT_CALL(data_source, Read(IsNull(), 0))
      .InSequence(seq)
      .WillOnce(Return(kReadSize));
  EXPECT_CALL(*mock_extractor, Read(Eq(ByRef(extractor_input)), IsNull()))
      .InSequence(seq)
      .WillOnce(Return(extractor::ExtractorInterface::RESULT_END_OF_INPUT));
  EXPECT_CALL(data_source, Close()).InSequence(seq).WillOnce(Return());

  EXPECT_THAT(cmc.GetNumBytesLoaded(), Eq(0));
  EXPECT_THAT(cmc.IsLoadCanceled(), Eq(false));
  EXPECT_THAT(cmc.Load(), Eq(true));
  extractor_input = nullptr;  // Probably was on the stack of cmc.Load()
  EXPECT_THAT(cmc.GetNumBytesLoaded(), Eq(kReadSize));
  EXPECT_THAT(cmc.IsLoadCanceled(), Eq(false));
}

TEST(ContainerMediaChunkTest, TestLoadFail) {
  upstream::Uri dummy_uri("dummy://");
  upstream::DataSpec data_spec(dummy_uri);
  util::Format format("", "", 0, 0, 0.0, 1, 0, 0, 0);
  constexpr int64_t kTestStartTime = 1;
  constexpr int64_t kTestEndTime = 2;
  constexpr int32_t kTestChunkIndex = 3;
  const base::TimeDelta kSampleOffset = base::TimeDelta::FromMicroseconds(5000);
  std::unique_ptr<MediaFormat> media_format(MediaFormat::CreateVideoFormat(
      "1", "video/mp4", "h264", 2200000, 32768, 1234567, 640, 480, nullptr, 16,
      45, 1.666));
  constexpr ssize_t kReadSize = 888;

  upstream::MockDataSource data_source;
  extractor::MockExtractor* mock_extractor = new extractor::MockExtractor;
  std::unique_ptr<extractor::ExtractorInterface> extractor(mock_extractor);
  scoped_refptr<ChunkExtractorWrapper> extractor_wrapper(
      new ChunkExtractorWrapper(std::move(extractor)));

  ContainerMediaChunk cmc(&data_source, &data_spec, Chunk::kTriggerUnspecified,
                          &format, kTestStartTime, kTestEndTime,
                          kTestChunkIndex, kSampleOffset, extractor_wrapper,
                          std::move(media_format), nullptr, true,
                          Chunk::kNoParentId);

  Sequence seq;
  extractor::ExtractorInputInterface* extractor_input = nullptr;

  EXPECT_CALL(data_source, Open(DebugStringEquals(data_spec.DebugString()), _))
      .InSequence(seq)
      .WillOnce(Return(0));
  EXPECT_CALL(*mock_extractor, Init(extractor_wrapper.get()))
      .InSequence(seq)
      .WillOnce(Return());
  EXPECT_CALL(*mock_extractor, Read(_, IsNull()))
      .InSequence(seq)
      .WillOnce(DoAll(SaveArg<0>(&extractor_input),
                      Return(extractor::ExtractorInterface::RESULT_CONTINUE)));
  EXPECT_CALL(*mock_extractor, Read(Eq(ByRef(extractor_input)), IsNull()))
      .InSequence(seq)
      .WillOnce(Invoke([](extractor::ExtractorInputInterface* input,
                          int64_t* seek_position) {
        input->Read(nullptr, 0);
        return extractor::ExtractorInterface::RESULT_CONTINUE;
      }));
  EXPECT_CALL(data_source, Read(IsNull(), 0))
      .InSequence(seq)
      .WillOnce(Return(kReadSize));
  EXPECT_CALL(*mock_extractor, Read(Eq(ByRef(extractor_input)), IsNull()))
      .InSequence(seq)
      .WillOnce(Return(extractor::ExtractorInterface::RESULT_IO_ERROR));
  EXPECT_CALL(data_source, Close()).InSequence(seq).WillOnce(Return());

  EXPECT_THAT(cmc.GetNumBytesLoaded(), Eq(0));
  EXPECT_THAT(cmc.IsLoadCanceled(), Eq(false));
  EXPECT_THAT(cmc.Load(), Eq(false));
  extractor_input = nullptr;  // Probably was on the stack of cmc.Load()
  EXPECT_THAT(cmc.GetNumBytesLoaded(), Eq(kReadSize));
  EXPECT_THAT(cmc.IsLoadCanceled(), Eq(false));
}

TEST(ContainerMediaChunkTest, TestLoadCancel) {
  upstream::Uri dummy_uri("dummy://");
  upstream::DataSpec data_spec(dummy_uri);
  util::Format format("", "", 0, 0, 0.0, 1, 0, 0, 0);
  constexpr int64_t kTestStartTime = 1;
  constexpr int64_t kTestEndTime = 2;
  constexpr int32_t kTestChunkIndex = 3;
  const base::TimeDelta kSampleOffset = base::TimeDelta::FromMicroseconds(5000);
  std::unique_ptr<MediaFormat> media_format(MediaFormat::CreateVideoFormat(
      "1", "video/mp4", "h264", 2200000, 32768, 1234567, 640, 480, nullptr, 16,
      45, 1.666));
  constexpr ssize_t kReadSize = 888;

  upstream::MockDataSource data_source;
  extractor::MockExtractor* mock_extractor = new extractor::MockExtractor;
  std::unique_ptr<extractor::ExtractorInterface> extractor(mock_extractor);
  scoped_refptr<ChunkExtractorWrapper> extractor_wrapper(
      new ChunkExtractorWrapper(std::move(extractor)));

  ContainerMediaChunk cmc(&data_source, &data_spec, Chunk::kTriggerUnspecified,
                          &format, kTestStartTime, kTestEndTime,
                          kTestChunkIndex, kSampleOffset, extractor_wrapper,
                          std::move(media_format), nullptr, true,
                          Chunk::kNoParentId);

  Sequence seq;
  extractor::ExtractorInputInterface* extractor_input = nullptr;

  EXPECT_CALL(data_source, Open(DebugStringEquals(data_spec.DebugString()), _))
      .InSequence(seq)
      .WillOnce(Return(0));
  EXPECT_CALL(*mock_extractor, Init(extractor_wrapper.get()))
      .InSequence(seq)
      .WillOnce(Return());
  EXPECT_CALL(*mock_extractor, Read(_, IsNull()))
      .InSequence(seq)
      .WillOnce(DoAll(SaveArg<0>(&extractor_input),
                      Return(extractor::ExtractorInterface::RESULT_CONTINUE)));
  EXPECT_CALL(*mock_extractor, Read(Eq(ByRef(extractor_input)), IsNull()))
      .InSequence(seq)
      .WillOnce(Invoke([&cmc](extractor::ExtractorInputInterface* input,
                              int64_t* seek_position) {
        input->Read(nullptr, 0);
        cmc.CancelLoad();
        return extractor::ExtractorInterface::RESULT_CONTINUE;
      }));
  EXPECT_CALL(data_source, Read(IsNull(), 0))
      .InSequence(seq)
      .WillOnce(Return(kReadSize));
  EXPECT_CALL(data_source, Close()).InSequence(seq).WillOnce(Return());

  EXPECT_THAT(cmc.GetNumBytesLoaded(), Eq(0));
  EXPECT_THAT(cmc.IsLoadCanceled(), Eq(false));
  EXPECT_THAT(cmc.Load(), Eq(false));
  extractor_input = nullptr;  // Probably was on the stack of cmc.Load()
  EXPECT_THAT(cmc.GetNumBytesLoaded(), Eq(kReadSize));
  EXPECT_THAT(cmc.IsLoadCanceled(), Eq(true));
}

}  // namespace chunk
}  // namespace ndash
