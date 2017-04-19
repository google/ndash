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

#include "extractor/stream_parser_extractor.h"
#include "base/memory/ref_counted.h"
#include "drm/drm_session_manager_mock.h"
#include "extractor/chunk_index_unittest.h"
#include "extractor/extractor_input.h"
#include "extractor/extractor_input_mock.h"
#include "extractor/extractor_output.h"
#include "extractor/extractor_output_mock.h"
#include "extractor/seek_map.h"
#include "extractor/track_output.h"
#include "extractor/track_output_mock.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "media_format.h"
#include "mp4/audio_decoder_config.h"
#include "mp4/channel_layout.h"
#include "mp4/encryption_scheme.h"
#include "mp4/media_track.h"
#include "mp4/media_tracks.h"
#include "mp4/rect.h"
#include "mp4/size.h"
#include "mp4/stream_parser_buffer.h"
#include "mp4/video_codecs.h"
#include "mp4/video_decoder_config.h"
#include "mp4/video_types.h"
#include "test/stream_parser_mock.h"
#include "util/util.h"

namespace ndash {
namespace extractor {

using ::testing::ByRef;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::Ge;
using ::testing::IsNull;
using ::testing::Mock;
using ::testing::NotNull;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::Sequence;
using ::testing::SetArgPointee;
using ::testing::StrictMock;
using ::testing::Values;
using ::testing::_;

class StreamParserExtractorTest : public ::testing::TestWithParam<std::string> {
 public:
  void SetUp() {
    mock_stream_parser_ = new media::StreamParserMock();
    stream_parser_.reset(mock_stream_parser_);
    media_log_ = new media::MediaLog();
  }

  void TearDown() {
    mock_stream_parser_ = nullptr;
    stream_parser_.reset();
    media_log_ = nullptr;
  }

 protected:
  void CaptureInitCallbacks() {
    EXPECT_CALL(*mock_stream_parser_,
                Init(_, _, _, true, _, _, _, _, Eq(media_log_)))
        .WillOnce(DoAll(SaveArg<0>(&init_cb_), SaveArg<1>(&config_cb_),
                        SaveArg<2>(&new_buffers_cb_),
                        SaveArg<4>(&encrypted_media_init_data_cb_),
                        SaveArg<5>(&new_segment_cb_),
                        SaveArg<6>(&end_of_segment_cb_),
                        SaveArg<7>(&new_sidx_cb_)));
  }
  void DiscardInitCallbacks() {
    EXPECT_CALL(*mock_stream_parser_,
                Init(_, _, _, true, _, _, _, _, Eq(media_log_)))
        .WillOnce(Return());
  }

  static ExtractorOutputInterface* GetOutput(StreamParserExtractor* spe) {
    return spe->output_;
  }

  media::StreamParser::InitCB init_cb_;
  media::StreamParser::NewConfigCB config_cb_;
  media::StreamParser::NewBuffersCB new_buffers_cb_;
  media::StreamParser::EncryptedMediaInitDataCB encrypted_media_init_data_cb_;
  media::StreamParser::NewMediaSegmentCB new_segment_cb_;
  media::StreamParser::EndMediaSegmentCB end_of_segment_cb_;
  media::StreamParser::NewSIDXCB new_sidx_cb_;

  media::StreamParserMock* mock_stream_parser_;
  scoped_refptr<media::MediaLog> media_log_;
  std::unique_ptr<media::StreamParser> stream_parser_;
};

TEST_F(StreamParserExtractorTest, InitRelese) {
  DiscardInitCallbacks();

  StrictMock<MockExtractorOutput> output;
  drm::MockDrmSessionManager drm_session_manager_mock;
  StreamParserExtractor spe(&drm_session_manager_mock,
                            std::move(stream_parser_), media_log_);
  spe.Init(&output);
  spe.Release();
}

TEST_F(StreamParserExtractorTest, Sniff) {
  DiscardInitCallbacks();

  StrictMock<MockExtractorInput> input;
  drm::MockDrmSessionManager drm_session_manager_mock;
  StreamParserExtractor spe(&drm_session_manager_mock,
                            std::move(stream_parser_), media_log_);
  Mock::VerifyAndClearExpectations(mock_stream_parser_);

  EXPECT_THAT(spe.Sniff(&input), Eq(true));
}

TEST_F(StreamParserExtractorTest, Read) {
  DiscardInitCallbacks();

  StrictMock<MockExtractorInput> input;
  drm::MockDrmSessionManager drm_session_manager_mock;
  StreamParserExtractor spe(&drm_session_manager_mock,
                            std::move(stream_parser_), media_log_);
  Mock::VerifyAndClearExpectations(mock_stream_parser_);

  constexpr ssize_t kReadSize = 876;
  void* buf;

  // Success reading something
  {
    Sequence seq;
    EXPECT_CALL(input, Read(_, Ge(kReadSize)))
        .InSequence(seq)
        .WillOnce(DoAll(SaveArg<0>(&buf), Return(kReadSize)));
    EXPECT_CALL(*mock_stream_parser_, Parse(Eq(ByRef(buf)), kReadSize))
        .InSequence(seq)
        .WillOnce(Return(true));
  }

  EXPECT_THAT(spe.Read(&input, nullptr),
              Eq(ExtractorInterface::RESULT_CONTINUE));

  Mock::VerifyAndClearExpectations(&input);
  Mock::VerifyAndClearExpectations(mock_stream_parser_);

  // Parse failure
  {
    Sequence seq;
    EXPECT_CALL(input, Read(_, Ge(kReadSize)))
        .InSequence(seq)
        .WillOnce(DoAll(SaveArg<0>(&buf), Return(kReadSize)));
    EXPECT_CALL(*mock_stream_parser_, Parse(Eq(ByRef(buf)), kReadSize))
        .InSequence(seq)
        .WillOnce(Return(false));
  }

  EXPECT_THAT(spe.Read(&input, nullptr),
              Eq(ExtractorInterface::RESULT_IO_ERROR));

  Mock::VerifyAndClearExpectations(&input);
  Mock::VerifyAndClearExpectations(mock_stream_parser_);

  // The rest of the cases expect no Parse() calls
  EXPECT_CALL(*mock_stream_parser_, Parse(_, _)).Times(0);

  // 0 bytes read
  EXPECT_CALL(input, Read(_, _)).WillOnce(Return(0));

  EXPECT_THAT(spe.Read(&input, nullptr),
              Eq(ExtractorInterface::RESULT_CONTINUE));

  Mock::VerifyAndClearExpectations(&input);

  // EOF
  EXPECT_CALL(input, Read(_, _))
      .WillOnce(Return(ExtractorInterface::RESULT_END_OF_INPUT));

  EXPECT_THAT(spe.Read(&input, nullptr),
              Eq(ExtractorInterface::RESULT_END_OF_INPUT));

  Mock::VerifyAndClearExpectations(&input);

  // Error
  EXPECT_CALL(input, Read(_, _))
      .WillOnce(Return(ExtractorInterface::RESULT_IO_ERROR));

  EXPECT_THAT(spe.Read(&input, nullptr),
              Eq(ExtractorInterface::RESULT_IO_ERROR));

  Mock::VerifyAndClearExpectations(&input);
}

TEST_F(StreamParserExtractorTest, InitAndRelease) {
  MockExtractorOutput output;

  DiscardInitCallbacks();
  drm::MockDrmSessionManager drm_session_manager_mock;
  StreamParserExtractor spe(&drm_session_manager_mock,
                            std::move(stream_parser_), media_log_);

  EXPECT_THAT(GetOutput(&spe), IsNull());
  spe.Init(&output);
  EXPECT_THAT(GetOutput(&spe), Eq(&output));
  spe.Release();
  EXPECT_THAT(GetOutput(&spe), IsNull());
}

TEST_F(StreamParserExtractorTest, EmptyCallbacks) {
  // At the time of this writing, NewMediaSegmentF and EndMediaSegmentF
  // do nothing.

  CaptureInitCallbacks();
  drm::MockDrmSessionManager drm_session_manager_mock;
  StreamParserExtractor spe(&drm_session_manager_mock,
                            std::move(stream_parser_), media_log_);
  Mock::VerifyAndClearExpectations(mock_stream_parser_);

  new_segment_cb_.Run();
  end_of_segment_cb_.Run();
}

TEST_P(StreamParserExtractorTest, SingleTrackConfigAndBuffers) {
  std::string flags = GetParam();

  bool is_audio = flags.find("audio") != std::string::npos;
  bool is_encrypted = flags.find("encrypted") != std::string::npos;

  std::unique_ptr<media::MediaTracks> media_tracks(new media::MediaTracks);
  media::StreamParser::TextTrackConfigMap text_track_config;
  StrictMock<MockExtractorOutput> extractor_out;
  StrictMock<MockTrackOutput> track_out;
  const MediaFormat* actual_media_format = nullptr;
  media::DemuxerStream::Type stream_type;
  const base::TimeDelta kTotalDuration = base::TimeDelta::FromSeconds(100);
  std::unique_ptr<MediaFormat> expected_media_format;

  // For encrypted tests.
  const std::string key_id = "key1234";
  const std::string iv = "0123456789abcdef";
  std::vector<media::SubsampleEntry> subsamples;
  subsamples.push_back(media::SubsampleEntry(32, 128));

  if (is_audio) {
    constexpr int kSamplesPerSecond = 9987;

    media::AudioDecoderConfig config(
        media::kCodecAAC, media::kSampleFormatS16, media::CHANNEL_LAYOUT_STEREO,
        kSamplesPerSecond, std::vector<uint8_t>(), media::EncryptionScheme());
    EXPECT_THAT(config.IsValidConfig(), Eq(true));
    media_tracks->AddAudioTrack(config, "only_track", "kind", "label",
                                "language");
    expected_media_format = MediaFormat::CreateAudioFormat(
        "only_track", "audio/mp4", "aac", kNoValue, kNoValue,
        kTotalDuration.InMicroseconds(), 2, kSamplesPerSecond, nullptr, 0,
        "language", kNoValue, DashChannelLayout::kChannelLayoutStereo,
        DashSampleFormat::kSampleFormatS16);
    stream_type = media::DemuxerStream::AUDIO;
  } else {
    constexpr int kVideoWidth = 640;
    constexpr int kVideoHeight = 480;

    media::VideoDecoderConfig config(
        media::kCodecMPEG4, media::VIDEO_CODEC_PROFILE_UNKNOWN,
        media::PIXEL_FORMAT_UNKNOWN, media::COLOR_SPACE_UNSPECIFIED,
        media::Size(kVideoWidth, kVideoHeight),
        media::Rect(kVideoWidth, kVideoHeight),
        media::Size(kVideoWidth, kVideoHeight), std::vector<uint8_t>(),
        media::EncryptionScheme());
    EXPECT_THAT(config.IsValidConfig(), Eq(true));
    media_tracks->AddVideoTrack(config, "only_track", "kind", "label",
                                "language");
    expected_media_format = MediaFormat::CreateVideoFormat(
        "only_track", "video/mp4", "h264", kNoValue, kNoValue,
        kTotalDuration.InMicroseconds(), kVideoWidth, kVideoHeight, nullptr, 0,
        0, 1.0);
    stream_type = media::DemuxerStream::VIDEO;
  }

  {
    Sequence seq;

    EXPECT_CALL(extractor_out, RegisterTrack(0))
        .InSequence(seq)
        .WillOnce(Return(&track_out));
    EXPECT_CALL(track_out, GiveFormatMock(NotNull()))
        .InSequence(seq)
        .WillOnce(SaveArg<0>(&actual_media_format));
    EXPECT_CALL(extractor_out, DoneRegisteringTracks())
        .InSequence(seq)
        .WillOnce(Return());
  }

  CaptureInitCallbacks();
  drm::MockDrmSessionManager drm_session_manager_mock;
  StreamParserExtractor spe(&drm_session_manager_mock,
                            std::move(stream_parser_), media_log_);
  Mock::VerifyAndClearExpectations(mock_stream_parser_);

  media::StreamParser::InitParameters params(kTotalDuration);
  init_cb_.Run(params);

  EXPECT_CALL(drm_session_manager_mock, Request(_, _));

  spe.Init(&extractor_out);
  EXPECT_THAT(config_cb_.Run(std::move(media_tracks), text_track_config),
              Eq(true));
  EXPECT_THAT(*actual_media_format, Eq(*expected_media_format));

  // TODO(adewhurst): Add test case with multiple tracks?

  Mock::VerifyAndClearExpectations(&extractor_out);
  Mock::VerifyAndClearExpectations(&track_out);

  const base::TimeDelta kBufTimestamp =
      base::TimeDelta::FromMilliseconds(20000);
  const base::TimeDelta kBufTimestamp2 =
      base::TimeDelta::FromMilliseconds(20001);
  const base::TimeDelta kBufTimestamp3 =
      base::TimeDelta::FromMilliseconds(80000);
  const base::TimeDelta kBufTimestamp4 =
      base::TimeDelta::FromMilliseconds(90000);
  const base::TimeDelta kDuration = base::TimeDelta::FromSeconds(2);
  const base::TimeDelta kDuration2 = base::TimeDelta::FromSeconds(3);
  constexpr int64_t kBufSize = 234;
  constexpr int64_t kBufSize2 = 345;
  constexpr int64_t kFirstWriteBytes = 100;
  constexpr int64_t kSecondWriteBytes = 134;

  media::StreamParser::BufferQueue empty_buffer_queue;
  media::StreamParser::BufferQueue data_buffer_queue;
  media::StreamParser::TextBufferQueueMap text_queue_map;

  // 5 buffers are created:
  // 1. kBufSize, kDuration, kBufTimestamp, written in two chunks
  //    (kFirstWriteBytes and kSecondWriteBytes), keyframe
  // 2. kBufSize, kDuration, kBufTimestamp2, written in one chunk, not keyframe
  // 3. kBufSize, kDuration, kBufTimestamp3, error when writing sample data,
  //    not keyframe
  // 4. kBufSize2, kDuration2, kBufTimestamp4, written in one chunk, keyframe
  // 5. End of stream
  // TODO(adewhurst): Add test case with text tracks, side data, encryption,
  //                  maybe multiple tracks?
  if (is_encrypted) {
    const std::vector<uint8_t> init_data;
    EXPECT_CALL(extractor_out, SetDrmInitData(_)).Times(1);
    encrypted_media_init_data_cb_.Run(media::EmeInitDataType::CENC, init_data);
  }

  {
    uint8_t buf[kBufSize];
    memset(buf, 42, kFirstWriteBytes);
    memset(buf + kFirstWriteBytes, 41, kSecondWriteBytes);
    data_buffer_queue.push_back(media::StreamParserBuffer::CopyFrom(
        buf, kBufSize, true, stream_type, 0));
    data_buffer_queue.back()->set_timestamp(kBufTimestamp);
    data_buffer_queue.back()->set_duration(kDuration);
    if (is_encrypted) {
      std::unique_ptr<media::DecryptConfig> decrypt_config(
          new media::DecryptConfig(key_id, iv, subsamples));
      data_buffer_queue.back()->set_decrypt_config(std::move(decrypt_config));
    }
  }
  const void* copied_buf = data_buffer_queue.back()->data();
  const void* copied_buf_part =
      data_buffer_queue.back()->data() + kFirstWriteBytes;

  {
    uint8_t buf2[kBufSize];
    memset(buf2, 43, sizeof(buf2));
    data_buffer_queue.push_back(media::StreamParserBuffer::CopyFrom(
        buf2, kBufSize, false, stream_type, 0));
    data_buffer_queue.back()->set_timestamp(kBufTimestamp2);
    data_buffer_queue.back()->set_duration(kDuration);
    if (is_encrypted) {
      std::unique_ptr<media::DecryptConfig> decrypt_config(
          new media::DecryptConfig(key_id, iv, subsamples));
      data_buffer_queue.back()->set_decrypt_config(std::move(decrypt_config));
    }
  }
  const void* copied_buf2 = data_buffer_queue.back()->data();

  {
    uint8_t buf3[kBufSize];
    memset(buf3, 44, sizeof(buf3));
    data_buffer_queue.push_back(media::StreamParserBuffer::CopyFrom(
        buf3, kBufSize, false, stream_type, 0));
    data_buffer_queue.back()->set_timestamp(kBufTimestamp3);
    data_buffer_queue.back()->set_duration(kDuration);
    if (is_encrypted) {
      std::unique_ptr<media::DecryptConfig> decrypt_config(
          new media::DecryptConfig(key_id, iv, subsamples));
      data_buffer_queue.back()->set_decrypt_config(std::move(decrypt_config));
    }
  }
  const void* copied_buf3 = data_buffer_queue.back()->data();

  {
    uint8_t buf4[kBufSize2];
    memset(buf4, 45, sizeof(buf4));
    data_buffer_queue.push_back(media::StreamParserBuffer::CopyFrom(
        buf4, kBufSize2, true, stream_type, 0));
    data_buffer_queue.back()->set_timestamp(kBufTimestamp4);
    data_buffer_queue.back()->set_duration(kDuration2);
    if (is_encrypted) {
      std::unique_ptr<media::DecryptConfig> decrypt_config(
          new media::DecryptConfig(key_id, iv, subsamples));
      data_buffer_queue.back()->set_decrypt_config(std::move(decrypt_config));
    }
  }
  const void* copied_buf4 = data_buffer_queue.back()->data();

  data_buffer_queue.push_back(media::StreamParserBuffer::CreateEOSBuffer());

  Sequence seq;

  // Buffer 1
  EXPECT_CALL(track_out,
              WriteSampleDataFixThis(copied_buf, kBufSize, true, NotNull()))
      .InSequence(seq)
      .WillOnce(DoAll(SetArgPointee<3>(kFirstWriteBytes), Return(true)));
  EXPECT_CALL(track_out, WriteSampleDataFixThis(copied_buf_part,
                                                kBufSize - kFirstWriteBytes,
                                                true, NotNull()))
      .InSequence(seq)
      .WillOnce(DoAll(SetArgPointee<3>(kSecondWriteBytes), Return(true)));
  if (is_encrypted) {
    EXPECT_CALL(track_out,
                WriteSampleMetadata(
                    kBufTimestamp.InMicroseconds(), kDuration.InMicroseconds(),
                    util::kSampleFlagSync | util::kSampleFlagEncrypted,
                    kBufSize, 0, Pointee(Eq(key_id)), Pointee(Eq(iv)),
                    Not(IsNull()), Not(IsNull())));
  } else {
    EXPECT_CALL(track_out,
                WriteSampleMetadata(kBufTimestamp.InMicroseconds(),
                                    kDuration.InMicroseconds(),
                                    util::kSampleFlagSync, kBufSize, 0,
                                    IsNull(), IsNull(), IsNull(), IsNull()));
  }

  // Buffer 2
  EXPECT_CALL(track_out,
              WriteSampleDataFixThis(copied_buf2, kBufSize, true, NotNull()))
      .InSequence(seq)
      .WillOnce(DoAll(SetArgPointee<3>(kBufSize), Return(true)));
  if (is_encrypted) {
    EXPECT_CALL(track_out,
                WriteSampleMetadata(kBufTimestamp2.InMicroseconds(),
                                    kDuration.InMicroseconds(),
                                    util::kSampleFlagEncrypted, kBufSize, 0,
                                    Pointee(Eq(key_id)), Pointee(Eq(iv)),
                                    Not(IsNull()), Not(IsNull())));
  } else {
    EXPECT_CALL(track_out,
                WriteSampleMetadata(kBufTimestamp2.InMicroseconds(),
                                    kDuration.InMicroseconds(), 0, kBufSize, 0,
                                    IsNull(), IsNull(), IsNull(), IsNull()));
  }

  // Buffer 3
  EXPECT_CALL(track_out,
              WriteSampleDataFixThis(copied_buf3, kBufSize, true, NotNull()))
      .InSequence(seq)
      .WillOnce(DoAll(SetArgPointee<3>(kSecondWriteBytes), Return(false)));

  // Buffer 4
  EXPECT_CALL(track_out,
              WriteSampleDataFixThis(copied_buf4, kBufSize2, true, NotNull()))
      .InSequence(seq)
      .WillOnce(DoAll(SetArgPointee<3>(kBufSize2), Return(true)));
  if (is_encrypted) {
    EXPECT_CALL(
        track_out,
        WriteSampleMetadata(kBufTimestamp4.InMicroseconds(),
                            kDuration2.InMicroseconds(),
                            util::kSampleFlagSync | util::kSampleFlagEncrypted,
                            kBufSize2, 0, Pointee(Eq(key_id)), Pointee(Eq(iv)),
                            Not(IsNull()), Not(IsNull())));
  } else {
    EXPECT_CALL(track_out,
                WriteSampleMetadata(kBufTimestamp4.InMicroseconds(),
                                    kDuration2.InMicroseconds(),
                                    util::kSampleFlagSync, kBufSize2, 0,
                                    IsNull(), IsNull(), IsNull(), IsNull()));
  }

  EXPECT_THAT(new_buffers_cb_.Run(empty_buffer_queue, data_buffer_queue,
                                  text_queue_map),
              Eq(true));
}

INSTANTIATE_TEST_CASE_P(WithParameters,
                        StreamParserExtractorTest,
                        ::testing::Values("video_clear"
                                          "audio_clear"
                                          "video_encrypted"
                                          "audio_encrypted"));

TEST_F(StreamParserExtractorTest, NewSIDX) {
  const std::vector<ChunkIndexEntry> kTestData = {
      {100, 0, 1000, 0},
      {500, 100, 1000, 1000},
      {10000, 5000, 50, 5000},
      {20000, 9999, 500, 5050},
  };

  std::unique_ptr<std::vector<uint32_t>> sizes;
  std::unique_ptr<std::vector<uint64_t>> offsets;
  std::unique_ptr<std::vector<uint64_t>> durations_us;
  std::unique_ptr<std::vector<uint64_t>> times_us;

  GenerateChunkIndexVectors(kTestData, &sizes, &offsets, &durations_us,
                            &times_us);

  CaptureInitCallbacks();
  drm::MockDrmSessionManager drm_session_manager_mock;
  StreamParserExtractor spe(&drm_session_manager_mock,
                            std::move(stream_parser_), media_log_);
  Mock::VerifyAndClearExpectations(mock_stream_parser_);

  StrictMock<MockExtractorOutput> extractor_out;

  const SeekMapInterface* seek_map = nullptr;

  EXPECT_CALL(extractor_out, GiveSeekMapMock(NotNull()))
      .WillOnce(SaveArg<0>(&seek_map));

  spe.Init(&extractor_out);
  new_sidx_cb_.Run(std::move(sizes), std::move(offsets),
                   std::move(durations_us), std::move(times_us));

  EXPECT_THAT(seek_map->GetPosition(-10), Eq(kTestData.at(0).offset));
  EXPECT_THAT(seek_map->GetPosition(0), Eq(kTestData.at(0).offset));
  EXPECT_THAT(seek_map->GetPosition(100), Eq(kTestData.at(0).offset));
  EXPECT_THAT(seek_map->GetPosition(999), Eq(kTestData.at(0).offset));
  EXPECT_THAT(seek_map->GetPosition(1000), Eq(kTestData.at(1).offset));
  EXPECT_THAT(seek_map->GetPosition(1001), Eq(kTestData.at(1).offset));
  EXPECT_THAT(seek_map->GetPosition(4999), Eq(kTestData.at(1).offset));
  EXPECT_THAT(seek_map->GetPosition(5000), Eq(kTestData.at(2).offset));
  EXPECT_THAT(seek_map->GetPosition(5050), Eq(kTestData.at(3).offset));
  EXPECT_THAT(seek_map->GetPosition(30000), Eq(kTestData.at(3).offset));
}

}  // namespace extractor
}  // namespace ndash
