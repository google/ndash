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
#include <limits>
#include <string>

#include "base/memory/ptr_util.h"
#include "chunk/chunk_extractor_wrapper.h"
#include "dash/representation_holder.h"
#include "extractor/extractor.h"
#include "extractor/extractor_mock.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mpd/representation.h"
#include "mpd/segment_template.h"

namespace ndash {
namespace dash {

using ::testing::Eq;
using ::testing::IsNull;
using ::testing::Pointee;

namespace {
const base::TimeDelta kStartTime = base::TimeDelta::FromSeconds(10);
const base::TimeDelta kDuration = base::TimeDelta::FromHours(2);
const base::TimeDelta kEndTime = kStartTime + kDuration;

const char kContentId[] = "content_id";
const int64_t kRevisionId = 98;
const char kCustomCacheKey[] = "cache_key";

// Create a test representation that our representation holder can hold and/or
// be updated with.
std::unique_ptr<mpd::Representation> CreateTestRepresentation(size_t shift) {
  std::unique_ptr<std::string> media_base_uri(new std::string("http://media"));

  std::unique_ptr<mpd::UrlTemplate> init_template =
      mpd::UrlTemplate::Compile("http://host/init/$Number$");
  std::unique_ptr<mpd::UrlTemplate> media_template =
      mpd::UrlTemplate::Compile("segment/$RepresentationID$/$Number$/");

  std::unique_ptr<std::vector<mpd::SegmentTimelineElement>> timeline(
      new std::vector<mpd::SegmentTimelineElement>);

  // Push 6 timeline entries starting at shift
  for (int i = 0; i < 6; i++) {
    timeline->push_back(mpd::SegmentTimelineElement((i + shift) * 2500, 2500));
  }

  std::unique_ptr<mpd::SegmentTemplate> segment_template(
      new mpd::SegmentTemplate(
          std::move(media_base_uri), std::unique_ptr<mpd::RangedUri>(nullptr),
          1000, 0, shift /* start_number*/, 2500, std::move(timeline),
          std::move(init_template), std::move(media_template)));

  util::Format format("1", "", 0, 0, 0.0, 1, 0, 0, 200000);

  return mpd::Representation::NewInstance(kContentId, kRevisionId, format,
                                          std::move(segment_template),
                                          kCustomCacheKey);
}

}  // namespace

TEST(RepresentationHolderTest, Accessors) {
  extractor::MockExtractor* extractor = new extractor::MockExtractor();
  scoped_refptr<chunk::ChunkExtractorWrapper> chunk_extractor_wrapper(
      new chunk::ChunkExtractorWrapper(
          std::unique_ptr<extractor::ExtractorInterface>(extractor)));

  std::unique_ptr<mpd::Representation> representation =
      CreateTestRepresentation(0);

  RepresentationHolder rh(kStartTime, kDuration, representation.get(),
                          chunk_extractor_wrapper);
  const RepresentationHolder* rh_const = &rh;

  EXPECT_THAT(rh.extractor_wrapper().get(), Eq(chunk_extractor_wrapper.get()));
  EXPECT_THAT(rh_const->extractor_wrapper().get(),
              Eq(const_cast<const chunk::ChunkExtractorWrapper*>(
                  chunk_extractor_wrapper.get())));
  EXPECT_THAT(rh_const->representation(), Eq(representation.get()));
  EXPECT_THAT(rh_const->segment_index(), Eq(representation->GetIndex()));
  EXPECT_THAT(rh_const->media_format(), IsNull());
}

TEST(RepresentationHolderTest, UpdateRepresentation) {
  // In this test, we will simulate a LIVE moving window of 6 segments
  // each 2.5 seconds in duration and update the representation holder.

  extractor::MockExtractor* extractor = new extractor::MockExtractor();
  scoped_refptr<chunk::ChunkExtractorWrapper> chunk_extractor_wrapper(
      new chunk::ChunkExtractorWrapper(
          std::unique_ptr<extractor::ExtractorInterface>(extractor)));

  // Live Window = 0,1,2,3,4,5
  std::unique_ptr<mpd::Representation> representation1 =
      CreateTestRepresentation(0);

  RepresentationHolder rh(base::TimeDelta::FromMilliseconds(0),
                          base::TimeDelta::FromMilliseconds(15000),
                          representation1.get(), chunk_extractor_wrapper);

  // Simulate an update with an adjacent representation starting from where
  // the old one left off.  Continuation, no overlap case.  Total period
  // duration increases by another 15 seconds.

  // Live Window = 6,7,8,9,10,11
  std::unique_ptr<mpd::Representation> representation2 =
      CreateTestRepresentation(6);
  bool success = rh.UpdateRepresentation(
      base::TimeDelta::FromMilliseconds(30000), representation2.get());

  EXPECT_TRUE(success);
  EXPECT_EQ(6, rh.GetFirstSegmentNum());
  EXPECT_EQ(6, rh.GetFirstAvailableSegmentNum());
  EXPECT_EQ(11, rh.GetLastSegmentNum());

  // Simulate an update with an overlap at the half way point with the old
  // representation.  Total period duration increased by 7.5 seconds.
  // Live Window = 9,10,11,12,13,14
  std::unique_ptr<mpd::Representation> representation3 =
      CreateTestRepresentation(9);
  success = rh.UpdateRepresentation(base::TimeDelta::FromMilliseconds(37500),
                                    representation3.get());
  EXPECT_TRUE(success);
  // First available and last are still the same since we are simulating a
  // live window and we can never see beyond those boundaries.
  EXPECT_EQ(9, rh.GetFirstSegmentNum());
  EXPECT_EQ(9, rh.GetFirstAvailableSegmentNum());
  EXPECT_EQ(14, rh.GetLastSegmentNum());

  // Simulate an update that indicates we've fallen out of the live window
  // due to gap (we missed segment 15). Total period duration will be 52.5
  // seconds now but this update should fail.
  // Live Window = 16,17,18,19,20,21
  std::unique_ptr<mpd::Representation> representation4 =
      CreateTestRepresentation(16);
  success = rh.UpdateRepresentation(base::TimeDelta::FromMilliseconds(52500),
                                    representation4.get());
  EXPECT_FALSE(success);
}

TEST(RepresentationHolderTest, SegmentMethods) {
  extractor::MockExtractor* extractor = new extractor::MockExtractor();
  scoped_refptr<chunk::ChunkExtractorWrapper> chunk_extractor_wrapper(
      new chunk::ChunkExtractorWrapper(
          std::unique_ptr<extractor::ExtractorInterface>(extractor)));

  std::unique_ptr<mpd::Representation> representation =
      CreateTestRepresentation(0);

  RepresentationHolder rh(base::TimeDelta::FromMilliseconds(0),
                          base::TimeDelta::FromMilliseconds(15000),
                          representation.get(), chunk_extractor_wrapper);

  base::TimeDelta now;
  EXPECT_EQ(0, rh.GetSegmentNum(now));

  now += base::TimeDelta::FromMilliseconds(2500);
  EXPECT_EQ(1, rh.GetSegmentNum(now));

  EXPECT_EQ(base::TimeDelta::FromMilliseconds(0), rh.GetSegmentStartTime(0));
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(2500), rh.GetSegmentStartTime(1));

  EXPECT_EQ(base::TimeDelta::FromMilliseconds(2500), rh.GetSegmentEndTime(0));
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(5000), rh.GetSegmentEndTime(1));

  EXPECT_FALSE(rh.IsBeyondLastSegment(5));
  EXPECT_TRUE(rh.IsBeyondLastSegment(6));

  EXPECT_FALSE(rh.IsBeforeFirstSegment(0));
  EXPECT_TRUE(rh.IsBeforeFirstSegment(-1));

  EXPECT_EQ(5, rh.GetLastSegmentNum());
  EXPECT_EQ(0, rh.GetFirstSegmentNum());
  EXPECT_EQ(0, rh.GetFirstAvailableSegmentNum());
  EXPECT_EQ("http://media/segment/1/0/", rh.GetSegmentUri(0)->GetUriString());
  EXPECT_EQ("http://media/segment/1/1/", rh.GetSegmentUri(1)->GetUriString());
}

}  // namespace dash
}  // namespace ndash
