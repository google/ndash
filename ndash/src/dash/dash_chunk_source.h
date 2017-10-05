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

#ifndef NDASH_CHUNK_DASH_CHUNK_SOURCE_H_
#define NDASH_CHUNK_DASH_CHUNK_SOURCE_H_

#include <cstdint>
#include <list>
#include <map>
#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "chunk/chunk.h"
#include "chunk/chunk_source.h"
#include "chunk/format_evaluator.h"
#include "dash/exposed_track.h"
#include "mpd/adaptation_set.h"

namespace ndash {

class ManifestFetcher;
class PlaybackRate;
class TimeRangeInterface;
struct TrackCriteria;

namespace chunk {
class Chunk;
class ChunkExtractorWrapper;
class MediaChunk;
}  // namespace chunk

namespace drm {
class DrmSessionManagerInterface;
}  // namespace drm

namespace mpd {
class DashSegmentIndexInterface;
class MediaPresentationDescription;
class RangedUri;
class Representation;
}  // namespace mpd

namespace qoe {
class QoeManager;
}  // namespace qoe

namespace upstream {
class DataSourceInterface;
}  // namespace upstream

namespace dash {

class RepresentationHolder;
class PeriodHolder;

typedef std::map<int32_t, std::unique_ptr<PeriodHolder>>::iterator
    PeriodHolderIterator;

// A ChunkSource for DASH streams.
// This implementation currently supports fMP4, webm, webvtt and ttml.
// This implementation makes the following assumptions about multi-period
// manifests:
// 1. That new periods will contain the same representations as previous
//    periods (i.e. no new or missing representations), and
// 2. That representations are contiguous across multiple periods
class DashChunkSource : public chunk::ChunkSourceInterface {
 public:
  // Definition for a callback to be notified of available range changes.
  typedef base::Callback<void(const TimeRangeInterface& available_range)>
      AvailableRangeChangedCB;

  // Construct a new DashChunkSource.
  //
  // manifest_fetcher: A fetcher for the manifest, which must have already
  //                   successfully completed an initial load.
  // data_source: A DataSource suitable for loading the media data.
  // adaptive_format_evaluator: For adaptive tracks, selects from the available
  //                            formats.
  // live_edge_latency: For live streams, the delay that the playback should
  //                    lag behind the "live edge" (i.e. the end of the most
  //                    recently defined media in the manifest). Choosing a
  //                    small value will minimize latency introduced by the
  //                    player, however note that the value sets an upper bound
  //                    on the length of media that the player can buffer.
  //                    Hence a small value may increase the probability of
  //                    rebuffering and playback failures.
  // elapsed_realtime_offset: If known, an estimate of the instantaneous
  //                          difference between server-side unix time and
  //                          base::Clock::Now(), specified as the server's
  //                          unix time minus the local elapsed time. It
  //                          unknown, set to 0.
  // start_at_live_edge: True if the stream should start at the live edge;
  //                     false if it should at the beginning of the live
  //                     window.
  // range_changed_cb: A callback for when the available range changes.
  DashChunkSource(drm::DrmSessionManagerInterface* drm_session_manager,
                  ManifestFetcher* manifest_fetcher,
                  upstream::DataSourceInterface* data_source,
                  chunk::FormatEvaluatorInterface* adaptive_format_evaluator,
                  const mpd::AdaptationType& adaptation_type,
                  base::TimeDelta live_edge_latency,
                  base::TimeDelta elapsed_realtime_offset,
                  bool start_at_live_edge,
                  AvailableRangeChangedCB range_changed_cb,
                  const PlaybackRate* playback_rate,
                  qoe::QoeManager* qoe = nullptr);

  ~DashChunkSource() override;

  // Returns the position of the nearest chunk start to the target time (it can
  // adjust forwards or backwards)
  base::TimeDelta GetAdjustedSeek(base::TimeDelta target_position) const;

  // ChunkSource implementation.
  bool CanContinueBuffering() const override;
  bool Prepare() override;
  int64_t GetDurationUs() override;
  const std::string GetContentType() override;

  // Enable the source with the specified track criteria.
  //
  // This method should only be called after the source has been prepared, and
  // when the source is disabled.
  //
  //  track_criteria The TrackCriteria used to select a subset of adaptation
  //  sets. There is no requirement for the track criteria to narrow down
  //  selection at least one adaptation set. It may be the case that none
  //  match, in which case the period will simply never produce any media
  //  chunks. The adaptation set selected between periods does not have to
  //  be the same. If more than one adaptation set is a match, the set is
  //  ordered by their id attributes and the first one will be selected.
  //  Must not be null and point to a valid TrackCriteria object until
  //  the next time Disable() is called. It is expected the track criteria is
  //  not changed between Enable() / Disable() calls.
  //
  //  Note that the criteria may apply to attributes found on adaptation sets
  //  directly OR attributes found on representations under adaptation sets. In
  //  the later case, all representations must share the attribute and value
  //  being matched.  If not, this is considered an error.
  //
  //  In general, it is expected that changing the track selection
  //  criteria will not result in a seam-less transition since the chunk source
  //  must be disabled and enabled again with the new criteria.
  void Enable(const TrackCriteria* track_criteria) override;
  void ContinueBuffering(base::TimeDelta playback_position) override;
  void GetChunkOperation(std::deque<std::unique_ptr<chunk::MediaChunk>>* queue,
                         base::TimeDelta playback_position,
                         chunk::ChunkOperationHolder* out) override;
  void OnChunkLoadCompleted(chunk::Chunk* chunk) override;
  void OnChunkLoadError(const chunk::Chunk* chunk,
                        chunk::ChunkLoadErrorReason e) override;
  void Disable(std::deque<std::unique_ptr<chunk::MediaChunk>>* queue) override;

  std::unique_ptr<TimeRangeInterface> GetAvailableRange() const;

  void SetFormatGivenCallback(chunk::Chunk::FormatGivenCB format_given_cb) {
    format_given_cb_ = format_given_cb;
  }

 private:
  friend class DashChunkSourceTest;

  // For TESTING
  DashChunkSource(
      drm::DrmSessionManagerInterface* drm_session_manager,
      scoped_refptr<const mpd::MediaPresentationDescription> manifest,
      upstream::DataSourceInterface* data_source,
      chunk::FormatEvaluatorInterface* adaptive_format_evaluator,
      const mpd::AdaptationType& adaptation_type,
      const PlaybackRate* playback_rate,
      qoe::QoeManager* qoe = nullptr);

  DashChunkSource(
      drm::DrmSessionManagerInterface* drm_session_manager,
      ManifestFetcher* manifest_fetcher,
      scoped_refptr<const mpd::MediaPresentationDescription> initial_manifest,
      upstream::DataSourceInterface* data_source,
      chunk::FormatEvaluatorInterface* adaptive_format_evaluator,
      const mpd::AdaptationType& adaptation_type,
      std::unique_ptr<base::TickClock> clock,
      base::TimeDelta live_edge_latency,
      base::TimeDelta elapsed_realtime_offset,
      bool start_at_live_edge,
      AvailableRangeChangedCB range_changed_cb,
      const PlaybackRate* playback_rate,
      qoe::QoeManager* qoe = nullptr);

  static std::unique_ptr<MediaFormat> GetTrackFormat(
      mpd::AdaptationType adaptation_set_type,
      const util::Format* format,
      const std::string& media_mime_type,
      base::TimeDelta duration);
  static bool GetMediaMimeType(const util::Format& format,
                               std::string* out_str);
  static bool MimeTypeIsWebm(const std::string& mime_type);
  static bool MimeTypeIsRawText(const std::string& mime_type);

  static std::unique_ptr<chunk::Chunk> NewInitializationChunk(
      const mpd::RangedUri* initialization_uri,
      const mpd::RangedUri* index_uri,
      const mpd::Representation& representation,
      scoped_refptr<chunk::ChunkExtractorWrapper> extractor,
      upstream::DataSourceInterface* data_source,
      int32_t manifest_index,
      chunk::Chunk::TriggerReason trigger,
      chunk::Chunk::FormatGivenCB format_given_cb);

  static std::unique_ptr<chunk::Chunk> NewMediaChunk(
      const PeriodHolder& period_holder,
      RepresentationHolder* representation_holder,
      upstream::DataSourceInterface* data_source,
      std::unique_ptr<const MediaFormat> media_format,
      int32_t segment_num,
      chunk::Chunk::TriggerReason trigger,
      chunk::Chunk::FormatGivenCB format_given_cb);

  // Find the first period that can produce chunks given the position. The
  // returned period's start/end times do not necessarily surround the position
  // given.  The returned period simply represents the first period that can
  // produce chunks at the given time OR in the future.  If no period can
  // produce chunks, returns nullptr.
  // TODO(rmrossi): Add reverse flag to scan backwards.
  PeriodHolder* FindPeriodHolder(base::TimeDelta position);
  const PeriodHolder* FindPeriodHolder(base::TimeDelta position) const;

  void ProcessManifest(
      scoped_refptr<const mpd::MediaPresentationDescription> manifest);

  void NotifyAvailableRangeChanged(const TimeRangeInterface& seek_range);

  // Moves the given iterator to the next period that can produce chunks.
  // Returns true if a next period was found. False otherwise.
  bool MoveToNextPeriod(PeriodHolderIterator find_result,
                        PeriodHolder** period_holder,
                        chunk::ChunkOperationHolder* out,
                        int32_t previous_parent_id);
  // Moves the given iterator to the previous period that can produce chunks.
  // Returns true if a previous period was found. False otherwise.
  bool MoveToPrevPeriod(PeriodHolderIterator find_result,
                        PeriodHolder** period_holder,
                        chunk::ChunkOperationHolder* out,
                        int32_t previous_parent_id);

  AvailableRangeChangedCB range_changed_cb_;
  chunk::Chunk::FormatGivenCB format_given_cb_;

  upstream::DataSourceInterface* data_source_;
  chunk::FormatEvaluatorInterface* adaptive_format_evaluator_;

  chunk::FormatEvaluation evaluation_;
  const mpd::AdaptationType adaptation_type_;
  drm::DrmSessionManagerInterface* drm_session_manager_;
  ManifestFetcher* manifest_fetcher_;
  std::map<int32_t, std::unique_ptr<PeriodHolder>> period_holders_;
  int32_t next_period_holder_index_ = 0;
  std::unique_ptr<base::TickClock> clock_;
  base::TimeDelta live_edge_latency_;
  base::TimeDelta elapsed_realtime_offset_;
  bool live_ = false;

  scoped_refptr<const mpd::MediaPresentationDescription> current_manifest_;
  // Not null after the source has been enabled. Must point to externally owned
  // valid criteria object while the source is enabled.
  const TrackCriteria* track_criteria_;
  bool track_is_enabled_ = false;
  std::unique_ptr<TimeRangeInterface> available_range_;
  bool prepare_called_ = false;
  bool start_at_live_edge_;
  bool last_chunk_was_initialization_ = false;

  bool fatal_error_ = false;

  const PlaybackRate* const playback_rate_;
  qoe::QoeManager* qoe_;
};

}  // namespace dash
}  // namespace ndash

#endif  // NDASH_CHUNK_DASH_CHUNK_SOURCE_H_
