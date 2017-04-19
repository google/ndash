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

#include "dash/dash_chunk_source.h"
#include "base/memory/ref_counted.h"
#include "chunk/fixed_evaluator.h"
#include "chunk/initialization_chunk.h"
#include "dash/dash_track_selector_mock.h"
#include "drm/drm_session_manager_mock.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mpd/adaptation_set.h"
#include "mpd/media_presentation_description.h"
#include "mpd/multi_segment_base.h"
#include "mpd/period.h"
#include "mpd/representation.h"
#include "mpd/segment_list.h"
#include "mpd/segment_timeline_element.h"
#include "mpd/single_segment_base.h"
#include "playback_rate.h"
#include "time_range.h"
#include "track_criteria.h"
#include "util/util.h"

namespace ndash {
namespace dash {

using ::testing::Eq;
using ::testing::Invoke;
using ::testing::NotNull;

class DashChunkSourceTest : public ::testing::Test {
 public:
  static constexpr int64_t kVodDurationMs = 30000;

  static constexpr int64_t kLiveSegmentCount = 5;
  static constexpr int64_t kLiveSegmentDurationMs = 1000;
  static constexpr int64_t kLiveDurationMs =
      kLiveSegmentCount * kLiveSegmentDurationMs;
  static constexpr int64_t kLiveTimeshiftBufferDepthMs = kLiveDurationMs;

  static constexpr int32_t kMultiPeriodCount = 2;
  static constexpr int64_t kMultiPeriodVodDurationMs =
      kVodDurationMs * kMultiPeriodCount;
  static constexpr int64_t kMultiPeriodLiveDurationMs =
      kLiveDurationMs * kMultiPeriodCount;

  static constexpr int64_t kAvailabilityStartTimeMs = 60000;
  static constexpr int64_t kElapsedRealtimeOffsetMs = 1000;

  static constexpr int32_t kTallHeight = 200;
  static constexpr int32_t kWideWidth = 400;

  static constexpr int32_t kOmitForNone = -1;
  static constexpr int32_t kOmitForAll = -2;

  const util::Format kRegularVideo;
  const util::Format kTallVideo;
  const util::Format kWideVideo;

  DashChunkSourceTest()
      : kRegularVideo("1", "video/mp4", 480, 240, -1, 1, -1, -1, 1000),
        kTallVideo("2", "video/mp4", 100, kTallHeight, -1, 1, -1, -1, 1000),
        kWideVideo("3", "video/mp4", kWideWidth, 50, -1, 1, -1, -1, 1000) {}

 protected:
  std::unique_ptr<mpd::Representation> BuildVodRepresentation(
      const util::Format& format,
      size_t period_num) {
    // Media file format will be "p#.mp4" where # represents the period. This
    // is used to check expectations for the DoChunkOperation test below.
    std::string url = "p";
    url.append(std::to_string(period_num).c_str());
    url.append(".mp4");

    std::unique_ptr<std::string> base_uri(new std::string(url));
    std::unique_ptr<mpd::RangedUri> ranged_uri(
        new mpd::RangedUri(base_uri.get(), "", 0, 100));
    std::unique_ptr<mpd::SingleSegmentBase> segment_base(
        new mpd::SingleSegmentBase(std::move(ranged_uri), 1, 0,
                                   std::move(base_uri), 0, -1));
    return mpd::Representation::NewInstance("", 0, std::move(format),
                                            std::move(segment_base));
  }

  std::unique_ptr<mpd::Representation> BuildSegmentTimelineRepresentation(
      int64_t timeline_duration_ms,
      int64_t timeline_start_time_ms) {
    std::unique_ptr<std::vector<mpd::SegmentTimelineElement>> segment_timeline(
        new std::vector<mpd::SegmentTimelineElement>());
    std::unique_ptr<std::vector<mpd::RangedUri>> media_segments(
        new std::vector<mpd::RangedUri>());
    int64_t segment_start_time_ms = timeline_start_time_ms;
    int64_t byte_start = 0;
    // Create all but the last segment with kLiveSegmentDurationMs.
    int32_t segment_count =
        util::Util::CeilDivide(timeline_duration_ms, kLiveSegmentDurationMs);
    std::unique_ptr<std::string> base_uri(new std::string());
    for (int32_t i = 0; i < segment_count - 1; i++) {
      segment_timeline->emplace_back(segment_start_time_ms,
                                     kLiveSegmentDurationMs);
      media_segments->emplace_back(base_uri.get(), "", byte_start, 500);
      segment_start_time_ms += kLiveSegmentDurationMs;
      byte_start += 500;
    }
    // The final segment duration is calculated so that the total duration is
    // timelineDurationMs.
    int64_t final_segment_duration_ms =
        (timeline_start_time_ms + timeline_duration_ms) - segment_start_time_ms;
    segment_timeline->emplace_back(segment_start_time_ms,
                                   final_segment_duration_ms);
    media_segments->emplace_back(base_uri.get(), "", byte_start, 500);
    segment_start_time_ms += final_segment_duration_ms;
    byte_start += 500;
    // Construct the list.
    std::unique_ptr<mpd::MultiSegmentBase> segment_base(new mpd::SegmentList(
        std::move(base_uri), std::unique_ptr<mpd::RangedUri>(), 1000, 0, 0, 0,
        std::move(segment_timeline), std::move(media_segments)));
    return mpd::Representation::NewInstance(nullptr, 0, kRegularVideo,
                                            std::move(segment_base));
  }

  scoped_refptr<mpd::MediaPresentationDescription> BuildMpd(
      int64_t duration_ms,
      size_t num_periods,
      size_t omit_adaptation_set_for_period = kOmitForNone,
      bool live = false,
      bool limit_timeshift_buffer = false) {
    std::vector<std::unique_ptr<mpd::Period>> periods;
    int64_t period_start_ms = 0;
    for (int n = 0; n < num_periods; n++) {
      std::vector<std::unique_ptr<mpd::AdaptationSet>> adaptation_sets;
      if (n != omit_adaptation_set_for_period &&
          omit_adaptation_set_for_period != kOmitForAll) {
        std::vector<std::unique_ptr<mpd::Representation>> representations;
        representations.emplace_back(BuildVodRepresentation(kTallVideo, n));
        representations.emplace_back(BuildVodRepresentation(kWideVideo, n));

        adaptation_sets.emplace_back(new mpd::AdaptationSet(
            0, mpd::AdaptationType::VIDEO, &representations));
      }
      periods.emplace_back(
          new mpd::Period("period", period_start_ms, &adaptation_sets));
      period_start_ms += duration_ms / num_periods;
    }
    return new mpd::MediaPresentationDescription(
        kAvailabilityStartTimeMs, duration_ms, -1, live, -1,
        (limit_timeshift_buffer) ? kLiveTimeshiftBufferDepthMs : -1,
        std::unique_ptr<mpd::DescriptorType>(), "", &periods);
  }

  scoped_refptr<mpd::MediaPresentationDescription> BuildVodMpd(
      int num_periods,
      int omit_adaptation_set_for_period = kOmitForNone) {
    return BuildMpd(kVodDurationMs, num_periods, omit_adaptation_set_for_period,
                    false, false);
  }

  static void CheckAvailableRange(const TimeRangeInterface& seek_range,
                                  base::TimeDelta start_time,
                                  base::TimeDelta end_time) {
    auto seek_range_values = seek_range.GetCurrentBounds();
    EXPECT_THAT(seek_range_values.first, Eq(start_time));
    EXPECT_THAT(seek_range_values.second, Eq(end_time));
  }

  // Calls GetChunkOperation repeatedly as we moving forward in simulated time
  // Returns a log of the type of media chunk that was processed and the
  // url so it can be checked against expectations later.
  std::string ConsumeChunks(DashChunkSource* chunk_source) {
    std::deque<std::unique_ptr<chunk::MediaChunk>> queue;
    base::TimeDelta playback_position;
    chunk::ChunkOperationHolder out;

    // As we move forward in time (5s each iteration), we should encounter
    // one init chunk followed by one media chunk for each period.
    std::string log;
    while (true) {
      chunk_source->GetChunkOperation(&queue, playback_position, &out);

      if (out.IsEndOfStream()) {
        break;
      }

      if (out.GetChunk()->type() == chunk::Chunk::kTypeMediaInitialization) {
        // Saw an init chunk.
        log.append("i");
        chunk::InitializationChunk* init_chunk =
            static_cast<chunk::InitializationChunk*>(out.GetChunk());
        std::unique_ptr<const MediaFormat> vfmt =
            MediaFormat::CreateVideoFormat(
                "1", kRegularVideo.GetMimeType(), kRegularVideo.GetCodecs(),
                kRegularVideo.GetBitrate(), 0, 33, kRegularVideo.GetWidth(),
                kRegularVideo.GetHeight(), std::unique_ptr<char[]>(nullptr), 0,
                0, 0);
        init_chunk->GiveFormat(std::move(vfmt));
      } else {
        // Saw a media chunk.
        log.append("m");
        EXPECT_THAT(out.GetChunk()->type(), Eq(chunk::Chunk::kTypeMedia));

        // Move forward in time.
        playback_position += base::TimeDelta::FromMilliseconds(5000);
      }

      // Append the url and a ','
      log.append(out.GetChunk()->data_spec()->uri.uri().c_str());
      log.append(",");
      chunk_source->OnChunkLoadCompleted(out.GetChunk());
      out.SetChunk(nullptr);
    }
    return log;
  }

  std::string DoChunkTest(int omit_adaptation_set_for_period) {
    MockDashTrackSelector mock_track_selector;

    // Each period will be kVodDurationMs / 3 in duration.
    scoped_refptr<mpd::MediaPresentationDescription> mpd(
        BuildVodMpd(3, omit_adaptation_set_for_period));

    drm::MockDrmSessionManager drm_session_manager_mock;
    chunk::FixedEvaluator evaluator;
    PlaybackRate rate;
    DashChunkSource chunk_source(&drm_session_manager_mock, mpd.get(), nullptr,
                                 &evaluator, mpd::AdaptationType::VIDEO, &rate);

    EXPECT_THAT(chunk_source.Prepare(), Eq(true));
    TrackCriteria criteria("video/*");
    chunk_source.Enable(&criteria);

    return ConsumeChunks(&chunk_source);
  }

  std::unique_ptr<DashChunkSource> CreateChunkSource(
      drm::DrmSessionManagerInterface* drm_session_manager,
      mpd::MediaPresentationDescription* mpd,
      chunk::FixedEvaluator* evaluator,
      PlaybackRate rate) {
    std::unique_ptr<DashChunkSource> chunk_source(
        new DashChunkSource(drm_session_manager, mpd, nullptr, evaluator,
                            mpd::AdaptationType::VIDEO, &rate));
    return chunk_source;
  }
};

TEST_F(DashChunkSourceTest, GetAvailableRangeOnVod) {
  MockDashTrackSelector mock_track_selector;

  scoped_refptr<mpd::MediaPresentationDescription> mpd(BuildVodMpd(1));

  drm::MockDrmSessionManager drm_session_manager_mock;
  chunk::FixedEvaluator evaluator;
  PlaybackRate rate;
  std::unique_ptr<DashChunkSource> chunk_source =
      CreateChunkSource(&drm_session_manager_mock, mpd.get(), &evaluator, rate);

  EXPECT_THAT(chunk_source->Prepare(), Eq(true));
  TrackCriteria criteria("video/*");
  chunk_source->Enable(&criteria);
  std::unique_ptr<TimeRangeInterface> available_range =
      chunk_source->GetAvailableRange();

  CheckAvailableRange(*available_range, base::TimeDelta::FromMilliseconds(0),
                      base::TimeDelta::FromMilliseconds(kVodDurationMs));
}

TEST_F(DashChunkSourceTest, GetAdjustedSeek) {
  MockDashTrackSelector mock_track_selector;

  scoped_refptr<mpd::MediaPresentationDescription> mpd(BuildVodMpd(3));

  drm::MockDrmSessionManager drm_session_manager_mock;
  chunk::FixedEvaluator evaluator;
  PlaybackRate rate;
  std::unique_ptr<DashChunkSource> chunk_source =
      CreateChunkSource(&drm_session_manager_mock, mpd.get(), &evaluator, rate);

  EXPECT_THAT(chunk_source->Prepare(), Eq(true));
  TrackCriteria criteria("video/*");
  chunk_source->Enable(&criteria);

  // Seek to 0s is within period 1, expect 0s.
  EXPECT_THAT(chunk_source->GetAdjustedSeek(base::TimeDelta::FromSeconds(0)),
              Eq(base::TimeDelta::FromSeconds(0)));

  // Seek to 5s is within period 1, expect 0s.
  EXPECT_THAT(chunk_source->GetAdjustedSeek(base::TimeDelta::FromSeconds(5)),
              Eq(base::TimeDelta::FromSeconds(0)));

  // Seek to 13s is within period 2, expect 10s.
  EXPECT_THAT(chunk_source->GetAdjustedSeek(base::TimeDelta::FromSeconds(13)),
              Eq(base::TimeDelta::FromSeconds(10)));

  // Seek to 17s is within period 2, expect 10s.
  EXPECT_THAT(chunk_source->GetAdjustedSeek(base::TimeDelta::FromSeconds(17)),
              Eq(base::TimeDelta::FromSeconds(10)));

  // Seek to 21s is within period 3, expect 20s.
  EXPECT_THAT(chunk_source->GetAdjustedSeek(base::TimeDelta::FromSeconds(21)),
              Eq(base::TimeDelta::FromSeconds(20)));

  // Seek to 26s is within period 3, expect 20s.
  EXPECT_THAT(chunk_source->GetAdjustedSeek(base::TimeDelta::FromSeconds(26)),
              Eq(base::TimeDelta::FromSeconds(20)));

  // Seek before start time should result in no adjustment.
  EXPECT_THAT(chunk_source->GetAdjustedSeek(base::TimeDelta::FromSeconds(-10)),
              Eq(base::TimeDelta::FromSeconds(-10)));

  // Seek beyond end time should result in no adjustment.
  EXPECT_THAT(chunk_source->GetAdjustedSeek(base::TimeDelta::FromSeconds(62)),
              Eq(base::TimeDelta::FromSeconds(62)));
}

TEST_F(DashChunkSourceTest, GetChunkOperationMultiPeriod) {
  // All periods will be able to produce media chunks.
  std::string log = DoChunkTest(kOmitForNone);

  // Since we spend 5s in each 10s period, we should see 2 media chunks in
  // each period.
  EXPECT_THAT(log, Eq("ip0.mp4,mp0.mp4,mp0.mp4,ip1.mp4,mp1.mp4,mp1.mp4,ip2.mp4,"
                      "mp2.mp4,mp2.mp4,"));
}

TEST_F(DashChunkSourceTest, GetChunkOperationMultiPeriodNoRepsMiddlePeriod) {
  // The 2nd period (period 1) will not have any adaptation sets.
  std::string log = DoChunkTest(1);

  // The second period having no adaptation sets or representations should not
  // have caused a problem. We should see media chunks for period 2 being
  // produced even though we were in period 1's range for a while.
  EXPECT_THAT(
      log,
      Eq("ip0.mp4,mp0.mp4,mp0.mp4,ip2.mp4,mp2.mp4,mp2.mp4,mp2.mp4,mp2.mp4,"));
}

TEST_F(DashChunkSourceTest, GetChunkOperationMultiPeriodNoRepsFirstPeriod) {
  // The 1st period (period 0) will not have any adaptation sets.
  std::string log = DoChunkTest(0);

  // The first period having no adaptation sets or representations should not
  // have caused a problem. We should see media chunks for period 1 being
  // produced even though we were in period 0's range for a while.
  EXPECT_THAT(
      log,
      Eq("ip1.mp4,mp1.mp4,mp1.mp4,mp1.mp4,mp1.mp4,ip2.mp4,mp2.mp4,mp2.mp4,"));
}

TEST_F(DashChunkSourceTest, GetChunkOperationMultiPeriodNoRepsLastPeriod) {
  // The 1st period (period 0) will not have any adaptation sets.
  std::string log = DoChunkTest(2);

  // The last period having no adaptation sets or representations should not
  // have caused a problem. The last chunk before eos is signalled should be
  // after 2 period 1 media segments.
  EXPECT_THAT(log, Eq("ip0.mp4,mp0.mp4,mp0.mp4,ip1.mp4,mp1.mp4,mp1.mp4,"));
}

TEST_F(DashChunkSourceTest, GetChunkOperationMultiPeriodNoRepsAllPeriods) {
  // No periods will have any adaptation sets.
  std::string log = DoChunkTest(kOmitForAll);

  // We should have hit eos right away and no harm done.
  EXPECT_THAT(log, Eq(""));
}

// TODO(adewhurst): Port the other ExoPlayer DashChunkSource unit tests

}  // namespace dash
}  // namespace ndash
