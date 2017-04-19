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

#include "extractor/rawcc_parser_extractor.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "extractor/extractor_output.h"
#include "extractor/extractor_output_mock.h"
#include "extractor/track_output.h"
#include "extractor/track_output_mock.h"
#include "extractor/unbuffered_extractor_input.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "test/test_data.h"
#include "upstream/data_source.h"
#include "util/util.h"

namespace ndash {
namespace extractor {

namespace {

const char bad_header[] = {'B', 'A', 'R', 'F', 0x00, 0x00, 0x00, 0x00};

const char bad_version[] = {'R', 'C', 'C', 0x01, 0x01, 0x00, 0x00, 0x00};

const char good_packet[] = {
    'R', 'C', 'C', 0x01, 0x00, 0x00, 0x00, 0x00,
    // pts
    0x00, 0x00, 0x00, 0x01,
    // count
    0x05,
    // entry 1
    0x03, 0x80, 0x80,
    // entry 2
    0x03, 0x81, 0x81,
    // entry 3
    0x03, 0x82, 0x82,
    // entry 4
    0x03, 0x83, 0x83,
    // entry 5
    0x03, 0x84, 0x84,
};

const char bad_packet[] = {
    'R', 'C', 'C', 0x01, 0x00, 0x00, 0x00, 0x00,
    // pts
    0x00, 0x00, 0x00, 0x01,
    // bad count
    0xFF,
    // num entries does not match count
    0x03, 0x80, 0x80,
};

// Helper class to control how data is delivered to the parser.
class FakeDataSource : public upstream::DataSourceInterface {
 public:
  FakeDataSource() : pos_(0), next_data_(nullptr), next_data_len_(0) {}
  ssize_t Open(const upstream::DataSpec& data_spec,
               const base::CancellationFlag* cancel) override {
    return 0;
  }

  void Close() override {}

  ssize_t Read(void* buffer, size_t read_length) override {
    int avail = std::min(next_data_len_ - pos_, (int)read_length);
    if (avail == 0) {
      return upstream::RESULT_END_OF_INPUT;
    }
    memcpy(buffer, next_data_ + pos_, avail);
    pos_ += avail;
    return avail;
  }

  void SetNextReadSrc(const char* data, int len) {
    next_data_ = data;
    next_data_len_ = len;
    pos_ = 0;
  }

 private:
  int pos_ = 0;
  const char* next_data_;
  int next_data_len_;
};
}  // namespace

using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::IsNull;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrictMock;
using ::testing::_;

TEST(RawCCParserExtractorTest, InitRelease) {
  StrictMock<MockExtractorOutput> output;
  StrictMock<MockTrackOutput> track_out;
  RawCCParserExtractor rpe;

  EXPECT_CALL(output, RegisterTrack(0)).WillOnce(Return(&track_out));
  rpe.Init(&output);
  rpe.Release();
}

// Tests header check.
TEST(RawCCParserExtractorTest, BadHeader) {
  FakeDataSource data_source;
  StrictMock<MockExtractorOutput> output;
  StrictMock<MockTrackOutput> track_out;
  RawCCParserExtractor rpe;

  EXPECT_CALL(output, RegisterTrack(0)).WillOnce(Return(&track_out));
  rpe.Init(&output);

  UnbufferedExtractorInput input(&data_source, 0, sizeof(bad_header));

  data_source.SetNextReadSrc(&bad_header[0], sizeof(bad_header));

  int read_num = rpe.Read(&input, nullptr);
  EXPECT_EQ(ExtractorInterface::RESULT_IO_ERROR, read_num);
}

// Tests version check.
TEST(RawCCParserExtractorTest, BadVersion) {
  FakeDataSource data_source;
  StrictMock<MockExtractorOutput> output;
  StrictMock<MockTrackOutput> track_out;
  RawCCParserExtractor rpe;

  EXPECT_CALL(output, RegisterTrack(0)).WillOnce(Return(&track_out));
  rpe.Init(&output);

  UnbufferedExtractorInput input(&data_source, 0, sizeof(bad_version));

  data_source.SetNextReadSrc(&bad_version[0], sizeof(bad_version));

  int read_num = rpe.Read(&input, nullptr);
  EXPECT_EQ(ExtractorInterface::RESULT_IO_ERROR, read_num);
}

// Tests bad count doesn't do any harm.
TEST(RawCCParserExtractorTest, BadPacket) {
  FakeDataSource data_source;
  StrictMock<MockExtractorOutput> output;
  StrictMock<MockTrackOutput> track_out;
  RawCCParserExtractor rpe;

  EXPECT_CALL(output, RegisterTrack(0)).WillOnce(Return(&track_out));
  rpe.Init(&output);

  UnbufferedExtractorInput input(&data_source, 0, sizeof(bad_packet));

  data_source.SetNextReadSrc(&bad_packet[0], sizeof(bad_packet));

  // Make every write succeed completely.
  EXPECT_CALL(track_out, WriteSampleDataFixThis(_, 8, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<3>(8), Return(true)));

  int read_num = rpe.Read(&input, nullptr);
  EXPECT_EQ(ExtractorInterface::RESULT_CONTINUE, read_num);
}

// Tests a good packet returned in one read works.
TEST(RawCCParserExtractorTest, ParseWhole) {
  FakeDataSource data_source;
  StrictMock<MockExtractorOutput> output;
  StrictMock<MockTrackOutput> track_out;
  RawCCParserExtractor rpe;

  EXPECT_CALL(output, RegisterTrack(0)).WillOnce(Return(&track_out));
  rpe.Init(&output);

  UnbufferedExtractorInput input(&data_source, 0, sizeof(good_packet));

  data_source.SetNextReadSrc(&good_packet[0], sizeof(good_packet));

  // Make every write succeed completely.
  EXPECT_CALL(track_out, WriteSampleDataFixThis(_, 8, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<3>(8), Return(true)));

  // time will be in 45khz clock
  const base::TimeDelta pts = base::TimeDelta::FromMicroseconds(
      util::Util::ScaleLargeTimestamp(1, util::kMicrosPerMs, 45));

  // 5 samples each 8 bytes
  constexpr int64_t expected_size = 5 * 8;

  EXPECT_CALL(track_out,
              WriteSampleMetadata(pts.InMicroseconds(), 0,
                                  util::kSampleFlagSync, expected_size, 0,
                                  IsNull(), IsNull(), IsNull(), IsNull()));

  // Should parse the whole packet in one go.
  int read_num = rpe.Read(&input, nullptr);
  EXPECT_EQ(ExtractorInterface::RESULT_CONTINUE, read_num);

  // Next read attempt should indicate end of input.
  read_num = rpe.Read(&input, nullptr);
  EXPECT_EQ(ExtractorInterface::RESULT_END_OF_INPUT, read_num);
}

// Tests parser goes through all state transitions by giving 1 byte
// at a time.
TEST(RawCCParserExtractorTest, ParseInStages) {
  FakeDataSource data_source;
  StrictMock<MockExtractorOutput> output;
  StrictMock<MockTrackOutput> track_out;
  RawCCParserExtractor rpe;

  EXPECT_CALL(output, RegisterTrack(0)).WillOnce(Return(&track_out));
  rpe.Init(&output);

  UnbufferedExtractorInput input(&data_source, 0, sizeof(good_packet));

  // Make every write succeed completely.
  EXPECT_CALL(track_out, WriteSampleDataFixThis(_, 8, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<3>(8), Return(true)));

  // time will be in 45khz clock
  const base::TimeDelta pts = base::TimeDelta::FromMicroseconds(
      util::Util::ScaleLargeTimestamp(1, util::kMicrosPerMs, 45));

  // 5 entries each 8 bytes
  constexpr int64_t expected_size = 5 * 8;

  EXPECT_CALL(track_out,
              WriteSampleMetadata(pts.InMicroseconds(), 0,
                                  util::kSampleFlagSync, expected_size, 0,
                                  IsNull(), IsNull(), IsNull(), IsNull()));

  int pos = 0;
  int len = 1;
  do {
    if (pos >= sizeof(good_packet)) {
      // No more data to give.
      len = 0;
    }
    data_source.SetNextReadSrc(&good_packet[pos], len);
    int read_num = rpe.Read(&input, nullptr);
    EXPECT_TRUE(read_num != ExtractorInterface::RESULT_IO_ERROR);
    if (read_num == ExtractorInterface::RESULT_END_OF_INPUT) {
      break;
    }
    pos += 1;
  } while (true);
}

// Tests parsing a large rawcc stream.
TEST(RawCCParserExtractorTest, ParseFile) {
  base::FilePath path(FLAGS_test_data_path);
  path = path.AppendASCII("test/data/rawcc");

  std::string rawcc;
  ASSERT_TRUE(base::ReadFileToString(path, &rawcc));

  FakeDataSource data_source;
  StrictMock<MockExtractorOutput> output;
  StrictMock<MockTrackOutput> track_out;
  RawCCParserExtractor rpe;

  EXPECT_CALL(output, RegisterTrack(0)).WillOnce(Return(&track_out));
  rpe.Init(&output);

  UnbufferedExtractorInput input(&data_source, 0, rawcc.length());

  // Make every write succeed completely.
  EXPECT_CALL(track_out, WriteSampleDataFixThis(_, 8, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<3>(8), Return(true)));

  EXPECT_CALL(track_out,
              WriteSampleMetadata(_, _, util::kSampleFlagSync, _, 0, IsNull(),
                                  IsNull(), IsNull(), IsNull()))
      .Times(AnyNumber());

  int pos = 0;
  int len = 1024;
  do {
    if (pos >= rawcc.length()) {
      // No more data to give.
      len = 0;
    } else if (pos + len >= rawcc.length()) {
      len = rawcc.length() - pos;
    }
    data_source.SetNextReadSrc(&rawcc.c_str()[pos], len);
    int read_num = rpe.Read(&input, nullptr);
    EXPECT_TRUE(read_num != ExtractorInterface::RESULT_IO_ERROR);
    if (read_num == ExtractorInterface::RESULT_END_OF_INPUT) {
      break;
    }
    pos += len;
  } while (true);
}

// Tests we can reset half way through parsing and not get errors.
TEST(RawCCParserExtractorTest, Reset) {
  FakeDataSource data_source;
  StrictMock<MockExtractorOutput> output;
  StrictMock<MockTrackOutput> track_out;
  RawCCParserExtractor rpe;

  EXPECT_CALL(output, RegisterTrack(0)).WillOnce(Return(&track_out));
  rpe.Init(&output);

  UnbufferedExtractorInput input(&data_source, 0, sizeof(good_packet));

  // Only give 18 bytes of the good packet, leaving the parser waiting for
  // more data after having read the header and only the first sample.
  data_source.SetNextReadSrc(&good_packet[0], 18);

  // Make every write succeed completely.
  EXPECT_CALL(track_out, WriteSampleDataFixThis(_, 8, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<3>(8), Return(true)));

  // Should parse up to the beginning of the 2nd sample.
  int read_num = rpe.Read(&input, nullptr);
  EXPECT_EQ(ExtractorInterface::RESULT_CONTINUE, read_num);

  // Now call Seek which should reset the parser. Parse is expecting to start
  // again.
  rpe.Seek();

  // Give the whole packet this time.
  data_source.SetNextReadSrc(&good_packet[0], sizeof(good_packet));

  // time will be in 45khz clock
  const base::TimeDelta pts = base::TimeDelta::FromMicroseconds(
      util::Util::ScaleLargeTimestamp(1, util::kMicrosPerMs, 45));

  // 5 samples each 8 bytes
  constexpr int64_t expected_size = 5 * 8;

  EXPECT_CALL(track_out,
              WriteSampleMetadata(pts.InMicroseconds(), 0,
                                  util::kSampleFlagSync, expected_size, 0,
                                  IsNull(), IsNull(), IsNull(), IsNull()));

  read_num = rpe.Read(&input, nullptr);
  EXPECT_EQ(ExtractorInterface::RESULT_CONTINUE, read_num);
}

}  // namespace extractor
}  // namespace ndash
