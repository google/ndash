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

#include "media_format.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace ndash {

using ::testing::DoubleEq;
using ::testing::Eq;
using ::testing::IsNull;
using ::testing::NotNull;

namespace {
std::unique_ptr<char[]> MakeInitData() {
  std::unique_ptr<char[]> init_data(new char[16]);
  for (int i = 0; i < 16; i++) {
    init_data[i] = i;
  }
  return init_data;
}

void CheckVideoMediaFormat(const MediaFormat* mf,
                           bool subsample_offset,
                           bool adaptive,
                           bool has_drm) {
  ASSERT_THAT(mf, NotNull());
  EXPECT_THAT(mf->GetMimeType(), Eq("video/mp4"));
  EXPECT_THAT(mf->GetCodecs(), Eq("h264"));
  EXPECT_THAT(mf->GetDurationUs(), Eq(1234567));

  if (subsample_offset) {
    EXPECT_THAT(mf->GetSubsampleOffsetUs(), Eq(667));
  } else {
    EXPECT_THAT(mf->GetSubsampleOffsetUs(), Eq(kOffsetSampleRelative));
  }

  if (has_drm) {
    ASSERT_THAT(mf->GetInitializationData(), NotNull());
    ASSERT_THAT(mf->GetInitializationDataLen(), Eq(16));
    for (int i = 0; i < 16; i++) {
      EXPECT_THAT(mf->GetInitializationData()[i], Eq(i));
    }
  } else {
    EXPECT_THAT(mf->GetInitializationData(), IsNull());
    EXPECT_THAT(mf->GetInitializationDataLen(), Eq(0));
  }

  if (adaptive) {
    EXPECT_THAT(mf->GetTrackId(), Eq("aa"));
    EXPECT_THAT(mf->IsAdaptive(), Eq(true));
    EXPECT_THAT(mf->GetBitrate(), Eq(kNoValue));
    EXPECT_THAT(mf->GetMaxInputSize(), Eq(kNoValue));
    EXPECT_THAT(mf->GetWidth(), Eq(kNoValue));
    EXPECT_THAT(mf->GetHeight(), Eq(kNoValue));
    EXPECT_THAT(mf->GetRotationDegrees(), Eq(kNoValue));
    EXPECT_THAT(mf->GetPixelWidthHeightRatio(), Eq(kNoValue));
  } else {
    EXPECT_THAT(mf->GetTrackId(), Eq("1"));
    EXPECT_THAT(mf->IsAdaptive(), Eq(false));
    EXPECT_THAT(mf->GetBitrate(), Eq(2200000));
    EXPECT_THAT(mf->GetMaxInputSize(), Eq(32768));
    EXPECT_THAT(mf->GetWidth(), Eq(640));
    EXPECT_THAT(mf->GetHeight(), Eq(480));
    EXPECT_THAT(mf->GetRotationDegrees(), Eq(45));
    EXPECT_THAT(mf->GetPixelWidthHeightRatio(), DoubleEq(1.666));
  }
}

}  // namespace

TEST(MediaFormatUnitTests, VideoFormat) {
  std::unique_ptr<MediaFormat> mf = MediaFormat::CreateVideoFormat(
      "1", "video/mp4", "h264", 2200000, 32768, 1234567, 640, 480,
      MakeInitData(), 16, 45, 1.666);

  CheckVideoMediaFormat(mf.get(), false, false, true);

  // Copy constructor.
  MediaFormat mf2(*mf);
  CheckVideoMediaFormat(&mf2, false, false, true);

  // Copy with subsample offset
  std::unique_ptr<MediaFormat> mf4 = mf2.CopyWithSubsampleOffsetUs(667);
  CheckVideoMediaFormat(mf4.get(), true, false, true);

  // Copy as adaptive
  std::unique_ptr<MediaFormat> mf5 = mf4->CopyAsAdaptive("aa");
  // CopyAsAdaptive clobbers the subsample offset and DRM
  CheckVideoMediaFormat(mf5.get(), false, true, false);
}

TEST(MediaFormatUnitTests, VideoFormatWithoutDrm) {
  std::unique_ptr<MediaFormat> mf =
      MediaFormat::CreateVideoFormat("1", "video/mp4", "h264", 2200000, 32768,
                                     1234567, 640, 480, nullptr, 0, 45, 1.666);

  CheckVideoMediaFormat(mf.get(), false, false, false);

  // Copy constructor.
  MediaFormat mf2(*mf);
  CheckVideoMediaFormat(&mf2, false, false, false);

  // Copy with subsample offset
  std::unique_ptr<MediaFormat> mf4 = mf2.CopyWithSubsampleOffsetUs(667);
  CheckVideoMediaFormat(mf4.get(), true, false, false);

  // Copy as adaptive
  std::unique_ptr<MediaFormat> mf5 = mf4->CopyAsAdaptive("aa");
  // CopyAsAdaptive clobbers the subsample offset
  CheckVideoMediaFormat(mf5.get(), false, true, false);
}

TEST(MediaFormatUnitTests, AudioFormat) {
  std::unique_ptr<MediaFormat> mf = MediaFormat::CreateAudioFormat(
      "1", "audio/mp4", "aac", 256000, 32768, 1234567, 2, 48000, MakeInitData(),
      16, "en_US", 0, DashChannelLayout::kChannelLayout5_0_Back,
      DashSampleFormat::kSampleFormatS16);

  ASSERT_TRUE(mf.get() != nullptr);
  EXPECT_EQ("1", mf->GetTrackId());
  EXPECT_EQ("audio/mp4", mf->GetMimeType());
  EXPECT_EQ("aac", mf->GetCodecs());
  EXPECT_EQ(256000, mf->GetBitrate());
  EXPECT_EQ(32768, mf->GetMaxInputSize());
  EXPECT_EQ(1234567, mf->GetDurationUs());
  EXPECT_EQ(2, mf->GetChannelCount());
  EXPECT_EQ(48000, mf->GetSampleRate());
  EXPECT_TRUE(nullptr != mf->GetInitializationData());
  EXPECT_EQ(16, mf->GetInitializationDataLen());
  EXPECT_EQ("en_US", mf->GetLanguage());
  EXPECT_DOUBLE_EQ(0, mf->GetPcmEncoding());
  EXPECT_EQ(DashChannelLayout::kChannelLayout5_0_Back, mf->GetChannelLayout());
  EXPECT_EQ(DashSampleFormat::kSampleFormatS16, mf->GetSampleFormat());
}

TEST(MediaFormatUnitTests, TextFormat) {
  std::unique_ptr<MediaFormat> mf =
      MediaFormat::CreateTextFormat("1", "text/vtt", 256, 1234567, "en_US", 0);
  ASSERT_TRUE(mf.get() != nullptr);
  EXPECT_EQ("1", mf->GetTrackId());
  EXPECT_EQ("text/vtt", mf->GetMimeType());
  EXPECT_EQ(256, mf->GetBitrate());
  EXPECT_EQ(1234567, mf->GetDurationUs());
  EXPECT_EQ("en_US", mf->GetLanguage());
  EXPECT_EQ(0, mf->GetSubsampleOffsetUs());
}

TEST(MediaFormatUnitTests, ImageFormat) {
  std::unique_ptr<MediaFormat> mf = MediaFormat::CreateImageFormat(
      "1", "image/jpeg", 0, 1234567, MakeInitData(), 16, "en_US");
  ASSERT_TRUE(mf.get() != nullptr);
  EXPECT_EQ("1", mf->GetTrackId());
  EXPECT_EQ("image/jpeg", mf->GetMimeType());
  EXPECT_EQ(0, mf->GetBitrate());
  EXPECT_EQ(1234567, mf->GetDurationUs());
  EXPECT_TRUE(nullptr != mf->GetInitializationData());
  EXPECT_EQ(16, mf->GetInitializationDataLen());
  EXPECT_EQ("en_US", mf->GetLanguage());
}

TEST(MediaFormatUnitTests, MimeType) {
  std::unique_ptr<MediaFormat> mf =
      MediaFormat::CreateFormatForMimeType("1", "application/pdf", 0, 1234567);
  EXPECT_EQ("1", mf->GetTrackId());
  EXPECT_EQ("application/pdf", mf->GetMimeType());
  EXPECT_EQ(0, mf->GetBitrate());
  EXPECT_EQ(1234567, mf->GetDurationUs());
}

TEST(MediaFormatUnitTests, CopyWithSubsampleOffsetUs) {
  std::unique_ptr<MediaFormat> mf =
      MediaFormat::CreateTextFormat("1", "text/vtt", 256, 1234567, "en_US", 0);
  ASSERT_TRUE(mf.get() != nullptr);

  EXPECT_EQ(0, mf->GetSubsampleOffsetUs());

  std::unique_ptr<MediaFormat> mf2 = mf->CopyWithSubsampleOffsetUs(10);
  ASSERT_TRUE(mf2.get() != nullptr);

  EXPECT_EQ(0, mf->GetSubsampleOffsetUs());
  EXPECT_EQ(10, mf2->GetSubsampleOffsetUs());
}

}  // namespace ndash
