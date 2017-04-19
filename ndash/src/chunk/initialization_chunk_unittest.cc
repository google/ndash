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

#include "chunk/initialization_chunk.h"
#include "chunk/initialization_chunk_mock.h"
#include "drm/drm_init_data_mock.h"
#include "extractor/extractor_mock.h"
#include "extractor/seek_map_mock.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "media_format.h"
#include "upstream/data_source_mock.h"

namespace ndash {
namespace chunk {

using ::testing::_;
using ::testing::Eq;
using ::testing::IsNull;
using ::testing::Return;

TEST(InitializationChunkTest, CanInstantiateMock) {
  upstream::Uri dummy_uri("dummy://");
  upstream::DataSpec data_spec(dummy_uri);

  MockInitializationChunk mock_init_chunk(nullptr, &data_spec,
                                          Chunk::kTriggerUnspecified, nullptr,
                                          nullptr, Chunk::kNoParentId);

  EXPECT_CALL(mock_init_chunk, GetNumBytesLoaded()).Times(0);

  EXPECT_THAT(mock_init_chunk.type(), Eq(Chunk::kTypeMediaInitialization));
  EXPECT_THAT(mock_init_chunk.trigger(), Eq(Chunk::kTriggerUnspecified));
  EXPECT_THAT(mock_init_chunk.format(), IsNull());
  EXPECT_THAT(mock_init_chunk.data_spec()->DebugString(),
              Eq(data_spec.DebugString()));
  EXPECT_THAT(mock_init_chunk.parent_id(), Eq(Chunk::kNoParentId));
}

TEST(InitializationChunkTest, Accessors) {
  upstream::Uri dummy_uri("dummy://");
  upstream::DataSpec data_spec(dummy_uri);

  InitializationChunk init_chunk(nullptr, &data_spec,
                                 Chunk::kTriggerUnspecified, nullptr, nullptr,
                                 Chunk::kNoParentId);

  std::unique_ptr<const MediaFormat> format =
      MediaFormat::CreateTextFormat("track", "text/plain", 100, 5000);
  const MediaFormat* saved_format = format.get();

  EXPECT_THAT(init_chunk.HasFormat(), Eq(false));
  init_chunk.GiveFormat(std::move(format));
  EXPECT_THAT(init_chunk.HasFormat(), Eq(true));

  EXPECT_THAT(format.get(), IsNull());

  format = init_chunk.TakeFormat();
  EXPECT_THAT(format.get(), Eq(saved_format));
  EXPECT_THAT(init_chunk.HasFormat(), Eq(false));

  drm::MockDrmInitData* drm_init_data = new drm::MockDrmInitData;
  scoped_refptr<drm::RefCountedDrmInitData> drm_init_data_ref(drm_init_data);
  EXPECT_THAT(init_chunk.HasDrmInitData(), Eq(false));
  init_chunk.SetDrmInitData(drm_init_data_ref);
  EXPECT_THAT(init_chunk.HasDrmInitData(), Eq(true));
  EXPECT_THAT(init_chunk.GetDrmInitData(), Eq(drm_init_data));

  extractor::MockSeekMap* saved_seek_map = new extractor::MockSeekMap;
  std::unique_ptr<const extractor::SeekMapInterface> seek_map(saved_seek_map);
  EXPECT_THAT(init_chunk.HasSeekMap(), Eq(false));
  init_chunk.GiveSeekMap(std::move(seek_map));
  EXPECT_THAT(init_chunk.HasSeekMap(), Eq(true));

  EXPECT_THAT(seek_map.get(), IsNull());

  seek_map = init_chunk.TakeSeekMap();
  EXPECT_THAT(seek_map.get(), Eq(saved_seek_map));
  EXPECT_THAT(init_chunk.HasSeekMap(), Eq(false));
}

TEST(InitializationChunkTest, TestLoad) {
  upstream::Uri dummy_uri("dummy://");
  upstream::DataSpec data_spec(dummy_uri);
  upstream::MockDataSource data_source;
  std::unique_ptr<extractor::ExtractorInterface> extractor_interface(
      new extractor::MockExtractor());
  extractor::MockExtractor* mock_extractor =
      static_cast<extractor::MockExtractor*>(extractor_interface.get());

  ChunkExtractorWrapper wrapper(std::move(extractor_interface));

  InitializationChunk init_chunk(&data_source, &data_spec,
                                 Chunk::kTriggerUnspecified, nullptr, &wrapper,
                                 Chunk::kNoParentId);

  EXPECT_CALL(data_source, Open(_, _)).WillOnce(Return(0));
  EXPECT_CALL(*mock_extractor, Init(&wrapper)).WillOnce(Return());
  EXPECT_CALL(data_source, Close()).WillOnce(Return());

  EXPECT_CALL(*mock_extractor, Read(_, _))
      .WillOnce(Return(extractor::ExtractorInterface::RESULT_END_OF_INPUT));

  init_chunk.Load();

  // TODO(adewhurst): More comprehensive Load() test
}

}  // namespace chunk
}  // namespace ndash
