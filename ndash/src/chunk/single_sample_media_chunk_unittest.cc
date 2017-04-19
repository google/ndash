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

#include "base/memory/ref_counted.h"
#include "chunk/single_sample_media_chunk.h"
#include "drm/drm_init_data_mock.h"
#include "extractor/extractor_input.h"
#include "extractor/indexed_track_output_mock.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "media_format.h"
#include "test_matchers.h"
#include "upstream/constants.h"
#include "upstream/data_source_mock.h"
#include "upstream/data_spec.h"
#include "upstream/uri.h"
#include "util/format.h"
#include "util/util.h"

namespace ndash {
namespace chunk {

using ::testing::ByRef;
using ::testing::Eq;
using ::testing::Ge;
using ::testing::InvokeWithoutArgs;
using ::testing::IsNull;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::Sequence;
using ::testing::SetArgPointee;
using ::testing::_;

namespace {
constexpr int64_t kStartTime = 1000;
constexpr int64_t kEndTime = 1001000;
constexpr int32_t kChunkIndex = 5;
constexpr int64_t kDuration = 1000000;
static_assert(kDuration == kEndTime - kStartTime,
              "Duration must match start/end time");
constexpr int32_t kFirstSampleIndex = 23;
constexpr int64_t kReadBytes1 = 123;
constexpr int64_t kReadBytes2 = 234;
constexpr int64_t kTotalReadBytes = 357;
static_assert(kTotalReadBytes == kReadBytes1 + kReadBytes2,
              "Total bytes must match individual sizes");
}  // namespace

TEST(SingleSampleMediaChunkTest, Accessors) {
  upstream::MockDataSource data_source;
  upstream::Uri dummy_uri("dummy://");
  upstream::DataSpec data_spec(dummy_uri);
  util::Format format("", "", 0, 0, 0.0, 1, 0, 0, 0);
  std::unique_ptr<MediaFormat> sample_format =
      MediaFormat::CreateTextFormat("track", "text/plain", 100, 5000);
  drm::MockDrmInitData* drm_init_data = new drm::MockDrmInitData;
  scoped_refptr<drm::RefCountedDrmInitData> drm_init_data_ref(drm_init_data);

  SingleSampleMediaChunk ss_chunk(
      &data_source, &data_spec, Chunk::kTriggerUnspecified, &format, kStartTime,
      kEndTime, kChunkIndex, sample_format.get(), drm_init_data_ref,
      Chunk::kNoParentId);

  EXPECT_THAT(*ss_chunk.data_spec(),
              DebugStringEquals(data_spec.DebugString()));
  EXPECT_THAT(ss_chunk.trigger(), Eq(Chunk::kTriggerUnspecified));
  EXPECT_THAT(ss_chunk.start_time_us(), Eq(kStartTime));
  EXPECT_THAT(ss_chunk.end_time_us(), Eq(kEndTime));
  EXPECT_THAT(ss_chunk.chunk_index(), Eq(kChunkIndex));
  EXPECT_THAT(ss_chunk.GetMediaFormat(), Eq(sample_format.get()));
  EXPECT_THAT(ss_chunk.GetDrmInitData(), Eq(drm_init_data_ref));
  EXPECT_THAT(ss_chunk.parent_id(), Eq(Chunk::kNoParentId));
  EXPECT_THAT(ss_chunk.GetNumBytesLoaded(), Eq(0));
  EXPECT_THAT(ss_chunk.IsLoadCanceled(), Eq(false));
}

TEST(SingleSampleMediaChunkTest, TestLoadSuccess) {
  upstream::MockDataSource data_source;
  upstream::Uri dummy_uri("dummy://");
  upstream::DataSpec data_spec(dummy_uri);
  util::Format format("", "", 0, 0, 0.0, 1, 0, 0, 0);
  std::unique_ptr<MediaFormat> sample_format =
      MediaFormat::CreateTextFormat("track", "text/plain", 100, 5000);
  drm::MockDrmInitData* drm_init_data = new drm::MockDrmInitData;
  scoped_refptr<drm::RefCountedDrmInitData> drm_init_data_ref(drm_init_data);

  SingleSampleMediaChunk ss_chunk(
      &data_source, &data_spec, Chunk::kTriggerUnspecified, &format, kStartTime,
      kEndTime, kChunkIndex, sample_format.get(), drm_init_data_ref,
      Chunk::kNoParentId);

  extractor::MockIndexedTrackOutput mock_output;
  EXPECT_CALL(mock_output, GetWriteIndex())
      .WillRepeatedly(Return(kFirstSampleIndex));

  Sequence seq;
  extractor::ExtractorInputInterface* extractor_input = nullptr;

  EXPECT_CALL(data_source, Open(DebugStringEquals(data_spec.DebugString()), _))
      .InSequence(seq)
      .WillOnce(Return(0));
  EXPECT_CALL(mock_output, WriteSampleData(_, Ge(size_t(kReadBytes1)), true, _))
      .InSequence(seq)
      .WillOnce(DoAll(SaveArg<0>(&extractor_input),
                      SetArgPointee<3>(kReadBytes1), Return(true)));
  EXPECT_CALL(mock_output, WriteSampleData(Eq(ByRef(extractor_input)),
                                           Ge(size_t(kReadBytes2)), true, _))
      .InSequence(seq)
      .WillOnce(DoAll(SetArgPointee<3>(kReadBytes2), Return(true)));
  EXPECT_CALL(mock_output,
              WriteSampleData(Eq(ByRef(extractor_input)), _, true, _))
      .InSequence(seq)
      .WillOnce(
          DoAll(SetArgPointee<3>(upstream::RESULT_END_OF_INPUT), Return(true)));
  EXPECT_CALL(data_source, Close()).InSequence(seq).WillOnce(Return());
  EXPECT_CALL(mock_output,
              WriteSampleMetadata(kStartTime, kDuration, util::kSampleFlagSync,
                                  kTotalReadBytes, 0, IsNull(), IsNull(),
                                  IsNull(), IsNull()))
      .InSequence(seq)
      .WillOnce(Return());

  ss_chunk.Init(&mock_output);
  EXPECT_THAT(ss_chunk.Load(), Eq(true));
  extractor_input = nullptr;  // Probably was on the stack of ss_chunk.Load()

  EXPECT_THAT(ss_chunk.GetNumBytesLoaded(), Eq(kTotalReadBytes));
  EXPECT_THAT(ss_chunk.IsLoadCanceled(), Eq(false));
}

TEST(SingleSampleMediaChunkTest, TestLoadFail) {
  upstream::MockDataSource data_source;
  upstream::Uri dummy_uri("dummy://");
  upstream::DataSpec data_spec(dummy_uri);
  util::Format format("", "", 0, 0, 0.0, 1, 0, 0, 0);
  std::unique_ptr<MediaFormat> sample_format =
      MediaFormat::CreateTextFormat("track", "text/plain", 100, 5000);
  drm::MockDrmInitData* drm_init_data = new drm::MockDrmInitData;
  scoped_refptr<drm::RefCountedDrmInitData> drm_init_data_ref(drm_init_data);

  SingleSampleMediaChunk ss_chunk(
      &data_source, &data_spec, Chunk::kTriggerUnspecified, &format, kStartTime,
      kEndTime, kChunkIndex, sample_format.get(), drm_init_data_ref,
      Chunk::kNoParentId);

  extractor::MockIndexedTrackOutput mock_output;
  EXPECT_CALL(mock_output, GetWriteIndex())
      .WillRepeatedly(Return(kFirstSampleIndex));

  Sequence seq;
  extractor::ExtractorInputInterface* extractor_input = nullptr;

  EXPECT_CALL(data_source, Open(DebugStringEquals(data_spec.DebugString()), _))
      .InSequence(seq)
      .WillOnce(Return(0));
  EXPECT_CALL(mock_output, WriteSampleData(_, Ge(size_t(kReadBytes1)), true, _))
      .InSequence(seq)
      .WillOnce(DoAll(SaveArg<0>(&extractor_input),
                      SetArgPointee<3>(kReadBytes1), Return(true)));
  EXPECT_CALL(mock_output, WriteSampleData(Eq(ByRef(extractor_input)),
                                           Ge(size_t(kReadBytes2)), true, _))
      .InSequence(seq)
      .WillOnce(DoAll(SetArgPointee<3>(kReadBytes2), Return(false)));
  EXPECT_CALL(data_source, Close()).InSequence(seq).WillOnce(Return());
  EXPECT_CALL(mock_output, WriteSampleMetadata(_, _, _, _, _, _, _, _, _))
      .Times(0);

  ss_chunk.Init(&mock_output);
  EXPECT_THAT(ss_chunk.Load(), Eq(false));
  extractor_input = nullptr;  // Probably was on the stack of ss_chunk.Load()

  EXPECT_THAT(ss_chunk.GetNumBytesLoaded(), Eq(kReadBytes1));
  EXPECT_THAT(ss_chunk.IsLoadCanceled(), Eq(false));
}

TEST(SingleSampleMediaChunkTest, TestLoadCancel) {
  upstream::MockDataSource data_source;
  upstream::Uri dummy_uri("dummy://");
  upstream::DataSpec data_spec(dummy_uri);
  util::Format format("", "", 0, 0, 0.0, 1, 0, 0, 0);
  std::unique_ptr<MediaFormat> sample_format =
      MediaFormat::CreateTextFormat("track", "text/plain", 100, 5000);
  drm::MockDrmInitData* drm_init_data = new drm::MockDrmInitData;
  scoped_refptr<drm::RefCountedDrmInitData> drm_init_data_ref(drm_init_data);

  SingleSampleMediaChunk ss_chunk(
      &data_source, &data_spec, Chunk::kTriggerUnspecified, &format, kStartTime,
      kEndTime, kChunkIndex, sample_format.get(), drm_init_data_ref,
      Chunk::kNoParentId);

  extractor::MockIndexedTrackOutput mock_output;
  EXPECT_CALL(mock_output, GetWriteIndex())
      .WillRepeatedly(Return(kFirstSampleIndex));

  Sequence seq;
  extractor::ExtractorInputInterface* extractor_input = nullptr;

  EXPECT_CALL(data_source, Open(DebugStringEquals(data_spec.DebugString()), _))
      .InSequence(seq)
      .WillOnce(Return(0));
  EXPECT_CALL(mock_output, WriteSampleData(_, Ge(size_t(kReadBytes1)), true, _))
      .InSequence(seq)
      .WillOnce(DoAll(SaveArg<0>(&extractor_input),
                      SetArgPointee<3>(kReadBytes1),
                      InvokeWithoutArgs([&ss_chunk] { ss_chunk.CancelLoad(); }),
                      Return(true)));
  EXPECT_CALL(data_source, Close()).InSequence(seq).WillOnce(Return());
  EXPECT_CALL(mock_output, WriteSampleMetadata(_, _, _, _, _, _, _, _, _))
      .Times(0);

  ss_chunk.Init(&mock_output);
  EXPECT_THAT(ss_chunk.Load(), Eq(false));
  extractor_input = nullptr;  // Probably was on the stack of ss_chunk.Load()

  EXPECT_THAT(ss_chunk.GetNumBytesLoaded(), Eq(kReadBytes1));
  EXPECT_THAT(ss_chunk.IsLoadCanceled(), Eq(true));
}

}  // namespace chunk
}  // namespace ndash
