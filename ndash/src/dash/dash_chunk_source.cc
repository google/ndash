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

#include <map>
#include <memory>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "chunk/chunk.h"
#include "chunk/chunk_extractor_wrapper.h"
#include "chunk/container_media_chunk.h"
#include "chunk/initialization_chunk.h"
#include "chunk/single_sample_media_chunk.h"
#include "dash/dash_wrapping_segment_index.h"
#include "dash/period_holder.h"
#include "dash/representation_holder.h"
#include "drm/drm_init_data.h"
#include "extractor/chunk_index.h"
#include "manifest_fetcher.h"
#include "media_format.h"
#include "mpd/adaptation_set.h"
#include "mpd/dash_segment_index.h"
#include "mpd/media_presentation_description.h"
#include "mpd/period.h"
#include "mpd/ranged_uri.h"
#include "mpd/representation.h"
#include "playback_rate.h"
#include "qoe/qoe_manager.h"
#include "time_range.h"
#include "upstream/data_source.h"
#include "upstream/data_spec.h"
#include "upstream/uri.h"
#include "util/format.h"
#include "util/mime_types.h"
#include "util/util.h"

namespace ndash {
namespace dash {

namespace {
const char kFourccTTML[] = "stpp";
const char kFourccWebVTT[] = "wvtt";

void BehindLiveWindowError(qoe::QoeManager* qoe) {
  // Do nothing.
  LOG(ERROR) << "BehindLiveWindow";
  if (qoe) {
    qoe->ReportVideoError(qoe::VideoErrorCode::MEDIA_FETCH_ERROR,
                          "ChunkLoadError", false);
  }
}

}  // namespace

DashChunkSource::DashChunkSource(
    drm::DrmSessionManagerInterface* drm_session_manager,
    ManifestFetcher* manifest_fetcher,
    upstream::DataSourceInterface* data_source,
    chunk::FormatEvaluatorInterface* adaptive_format_evaluator,
    const mpd::AdaptationType& adaptation_type,
    base::TimeDelta live_edge_latency,
    base::TimeDelta elapsed_realtime_offset,
    bool start_at_live_edge,
    AvailableRangeChangedCB range_changed_cb,
    const PlaybackRate* playback_rate,
    qoe::QoeManager* qoe)
    : DashChunkSource(
          drm_session_manager,
          manifest_fetcher,
          manifest_fetcher->GetManifest(),
          data_source,
          adaptive_format_evaluator,
          adaptation_type,
          std::unique_ptr<base::TickClock>(new base::DefaultTickClock()),
          live_edge_latency,
          elapsed_realtime_offset,
          start_at_live_edge,
          range_changed_cb,
          playback_rate,
          qoe) {}

DashChunkSource::~DashChunkSource() {}

base::TimeDelta DashChunkSource::GetAdjustedSeek(
    base::TimeDelta target_position) const {
  base::TimeDelta new_position = target_position;
  const PeriodHolder* period_holder = FindPeriodHolder(target_position);

  if (period_holder == nullptr) {
    LOG(WARNING) << "Can't adjust seek, no period will produce media.";
    return new_position;
  }

  if (target_position < period_holder->start_time()) {
    LOG(WARNING) << "Can't adjust seek (" << target_position
                 << ") because it is before the period start time ("
                 << period_holder->start_time() << ")";
    return new_position;
  }

  const mpd::DashSegmentIndexInterface* segment_index =
      period_holder->GetArbitrarySegmentIndex();

  if (!segment_index) {
    // We don't have an index, so there's nothing that can be done
    LOG(INFO) << "Can't adjust seek (" << target_position << ") because there "
              << "is no index";
    return new_position;
  }

  base::TimeDelta target_in_period =
      target_position - period_holder->start_time();
  const base::TimeDelta* period_end = period_holder->GetAvailableEndTime();

  int32_t segment_num;
  int32_t last_segment_num;
  if (period_end) {
    segment_num = segment_index->GetSegmentNum(
        target_in_period.InMicroseconds(), period_end->InMicroseconds());
    last_segment_num =
        segment_index->GetLastSegmentNum(period_end->InMicroseconds());
  } else {
    // Unbounded; we'll fudge the numbers a little to figure out the segment
    // boundary.
    // TODO(adewhurst): We'll probably need to avoid jumping outside of the
    //                  live window when applicable.
    segment_num =
        segment_index->GetSegmentNum(target_in_period.InMicroseconds(),
                                     target_in_period.InMicroseconds() + 1);
    // TODO(adewhurst): Figure out how to deduce the next segment boundary. It
    //                  can be derived from MultiSegmentBase::duration_. For
    //                  now, pretend this is the last segment. This implies
    //                  that we'll just round down.
    last_segment_num = segment_num;
  }

  base::TimeDelta segment_start =
      base::TimeDelta::FromMicroseconds(segment_index->GetTimeUs(segment_num));

  // Default to rounding down
  new_position = segment_start;

  if (segment_num == last_segment_num) {
    // We are at the last segment, so it's impossible to round up. We're done.
    return period_holder->start_time() + new_position;
  }

  base::TimeDelta next_segment_start = base::TimeDelta::FromMicroseconds(
      segment_index->GetTimeUs(segment_num + 1));

  if (target_in_period - segment_start >
      next_segment_start - target_in_period) {
    // Closer to end of segment: round up
    new_position = next_segment_start;
  }

  return period_holder->start_time() + new_position;
}

bool DashChunkSource::CanContinueBuffering() const {
  if (fatal_error_) {
    return false;
  } else if (manifest_fetcher_) {
    return manifest_fetcher_->CanContinueBuffering();
  }

  return true;
}

bool DashChunkSource::Prepare() {
  if (!prepare_called_) {
    prepare_called_ = true;
  }
  return !fatal_error_;
}

int64_t DashChunkSource::GetDurationUs() {
  return live_ ? 0
               : base::TimeDelta::FromMilliseconds(
                     current_manifest_->GetDuration())
                     .InMicroseconds();
}

const std::string DashChunkSource::GetContentType() {
  switch (adaptation_type_) {
    case mpd::AdaptationType::VIDEO:
      return "video";
    case mpd::AdaptationType::AUDIO:
      return "audio";
    case mpd::AdaptationType::TEXT:
      return "text";
    default:
      return "unknown";
  }
}

void DashChunkSource::Enable(const TrackCriteria* track_criteria) {
  track_is_enabled_ = true;
  track_criteria_ = track_criteria;
  adaptive_format_evaluator_->Enable();

  if (manifest_fetcher_) {
    manifest_fetcher_->Enable();
    ProcessManifest(manifest_fetcher_->GetManifest());
  } else {
    ProcessManifest(current_manifest_.get());
  }
}

void DashChunkSource::ContinueBuffering(base::TimeDelta playback_position) {
  if (!manifest_fetcher_ || !current_manifest_->IsDynamic() || fatal_error_) {
    return;
  }

  if (manifest_fetcher_->HasManifest() &&
      manifest_fetcher_->GetManifest() != current_manifest_) {
    DLOG(INFO) << "New manifest";
    ProcessManifest(manifest_fetcher_->GetManifest());
  }

  // TODO: This is a temporary hack to avoid constantly refreshing the MPD in
  // cases where min_update_period is set to 0. In such cases we shouldn't
  // refresh unless there is explicit signaling in the stream, according to:
  // http://azure.microsoft.com/blog/2014/09/13/dash-live-streaming-with-azure-media-service/
  base::TimeDelta min_update_period = base::TimeDelta::FromMilliseconds(
      current_manifest_->GetMinUpdatePeriod());
  if (min_update_period.is_zero()) {
    min_update_period = base::TimeDelta::FromSeconds(5);
  }

  if (clock_->NowTicks() >
      manifest_fetcher_->GetManifestLoadStartTimestamp() + min_update_period) {
    manifest_fetcher_->RequestRefresh();
  }
}

void DashChunkSource::GetChunkOperation(
    std::deque<std::unique_ptr<chunk::MediaChunk>>* queue,
    base::TimeDelta playback_position,
    chunk::ChunkOperationHolder* out) {
  if (fatal_error_) {
    out->SetChunk(nullptr);
    return;
  }

  bool starting_new_period;
  PeriodHolder* period_holder;

  TimeRangeInterface::TimeDeltaPair bounds =
      available_range_->GetCurrentBounds();

  if (queue->empty()) {
    if (live_) {
      if (!playback_position.is_zero()) {
        // If the position is non-zero then assume the client knows where
        // it's seeking.
        start_at_live_edge_ = false;
      }
      if (start_at_live_edge_) {
        // We want live streams to start at the live edge instead of the
        // beginning of the manifest
        playback_position =
            std::max(bounds.first, bounds.second - live_edge_latency_);
      } else {
        // we subtract 1 from the upper bound because it's exclusive for that
        // bound
        playback_position =
            std::min(playback_position,
                     bounds.second - base::TimeDelta::FromMicroseconds(1));
        playback_position = std::max(playback_position, bounds.first);
      }
    }

    period_holder = FindPeriodHolder(playback_position);
    if (period_holder == nullptr) {
      if (!current_manifest_->IsDynamic()) {
        // The current manifest isn't dynamic, so we've reached the end
        // of the stream.
        out->SetEndOfStream(true);
      }
      out->SetChunk(nullptr);
      return;
    }
    starting_new_period = true;
  } else {
    if (start_at_live_edge_) {
      // now that we know the player is consuming media chunks (since the
      // queue isn't empty), set start_at_live_edge_ to false so that the
      // user can perform seek operations
      start_at_live_edge_ = false;
    }

    chunk::MediaChunk* previous = queue->back().get();
    base::TimeDelta next_segment_start_time =
        base::TimeDelta::FromMicroseconds(previous->end_time_us());
    if (live_ && next_segment_start_time < bounds.first) {
      // This is before the first chunk in the current manifest.
      out->SetChunk(nullptr);
      fatal_error_ = true;
      BehindLiveWindowError(qoe_);
      return;
    } else if (current_manifest_->IsDynamic() &&
               next_segment_start_time >= bounds.second) {
      // This chunk is beyond the last chunk in the current manifest. If the
      // index is bounded we'll need to wait until it's refreshed. If it's
      // unbounded we just need to wait for a while before attempting to load
      // the chunk.
      out->SetChunk(nullptr);
      return;
    } else {
      // A period's duration is the maximum of its various representation's
      // durations, so it's possible that due to the minor differences
      // between them our available range values might not sync exactly with
      // the actual available content, so double check whether or not we've
      // really run out of content to play.
      DCHECK(!period_holders_.empty());
      const PeriodHolder* last_period_holder =
          period_holders_.rbegin()->second.get();
      if (previous->parent_id() == last_period_holder->local_index()) {
        const RepresentationHolder* representation_holder =
            last_period_holder->GetRepresentationHolder(
                previous->format()->GetId());
        DCHECK(representation_holder);
        bool fell_off_end = playback_rate_->IsForward()
                                ? representation_holder->IsBeyondLastSegment(
                                      previous->GetNextChunkIndex())
                                : representation_holder->IsBeforeFirstSegment(
                                      previous->GetPrevChunkIndex());
        if (fell_off_end) {
          // Don't trip eos if we're tricking. Just chill.
          if (!current_manifest_->IsDynamic() && playback_rate_->IsNormal()) {
            // The current manifest isn't dynamic, so we've reached the end
            // of the stream.
            out->SetEndOfStream(true);
          }
          out->SetChunk(nullptr);
          return;
        }
      }
    }

    CHECK(!period_holders_.empty());

    starting_new_period = false;
    auto find_result = period_holders_.find(previous->parent_id());
    period_holder = (find_result == period_holders_.end())
                        ? nullptr
                        : find_result->second.get();

    if (period_holder == nullptr) {
      // The previous chunk was from a period that's no longer on the
      // manifest, therefore the next chunk must be the first one in the
      // first period that's still on the manifest (note that we can't
      // actually update the segment_num yet because the new period might
      // have a different sequence and its segment_index might not have been
      // loaded yet).
      period_holder = period_holders_.begin()->second.get();
      starting_new_period = true;
    } else if (!period_holder->index_is_unbounded()) {
      const RepresentationHolder* representation_holder =
          period_holder->GetRepresentationHolder(previous->format()->GetId());
      if (playback_rate_->IsForward() &&
          representation_holder->IsBeyondLastSegment(
              previous->GetNextChunkIndex())) {
        if (!MoveToNextPeriod(find_result, &period_holder, out,
                              previous->parent_id())) {
          return;
        }
        starting_new_period = true;
      } else if (!playback_rate_->IsForward() &&
                 representation_holder->IsBeforeFirstSegment(
                     previous->GetPrevChunkIndex())) {
        if (!MoveToPrevPeriod(find_result, &period_holder, out,
                              previous->parent_id())) {
          return;
        }
        starting_new_period = true;
      }
    }
  }

  evaluation_.queue_size_ = queue->size();
  if (!evaluation_.format_ || !last_chunk_was_initialization_) {
    std::vector<util::Format> formats;
    for (const RepresentationHolder& holder :
         period_holder->RepresentationHolderValues()) {
      formats.push_back(holder.representation()->GetFormat());
    }

    adaptive_format_evaluator_->Evaluate(*queue, playback_position, formats,
                                         &evaluation_, *playback_rate_);
  }

  const util::Format* selected_format = evaluation_.format_.get();
  out->SetQueueSize(evaluation_.queue_size_);

  chunk::Chunk* out_chunk = out->GetChunk();
  if (out_chunk) {
    DCHECK(out_chunk->format());
  }

  if (!selected_format) {
    out->SetChunk(nullptr);
    return;
  } else if (out->GetQueueSize() == queue->size() && out_chunk &&
             *out_chunk->format() == *selected_format) {
    // We already have a chunk, and the evaluation hasn't changed either the
    // format or the size of the queue. Leave unchanged.
    return;
  }

  RepresentationHolder* representation_holder =
      period_holder->GetRepresentationHolder(selected_format->GetId());
  const mpd::Representation* selected_representation =
      representation_holder->representation();

  const mpd::RangedUri* pending_initialization_uri = nullptr;
  const mpd::RangedUri* pending_index_uri = nullptr;

  const MediaFormat* media_format = representation_holder->media_format();
  if (!media_format) {
    pending_initialization_uri =
        selected_representation->GetInitializationUri();
  }
  if (!representation_holder->segment_index()) {
    pending_index_uri = selected_representation->GetIndexUri();
  }

  if (pending_initialization_uri || pending_index_uri) {
    // We have initialization and/or index requests to make.
    std::unique_ptr<chunk::Chunk> initialization_chunk = NewInitializationChunk(
        pending_initialization_uri, pending_index_uri, *selected_representation,
        representation_holder->extractor_wrapper(), data_source_,
        period_holder->local_index(), evaluation_.trigger_, format_given_cb_);
    last_chunk_was_initialization_ = true;
    out->SetChunk(std::move(initialization_chunk));
    return;
  }

  int segment_num =
      queue->empty()
          ? representation_holder->GetSegmentNum(playback_position)
          : starting_new_period
                ? representation_holder->GetFirstAvailableSegmentNum()
                : playback_rate_->IsForward()
                      ? queue->back()->GetNextChunkIndex()
                      : queue->back()->GetPrevChunkIndex();

  std::unique_ptr<chunk::Chunk> next_media_chunk = NewMediaChunk(
      *period_holder, representation_holder, data_source_, media_format,
      segment_num, evaluation_.trigger_, format_given_cb_);
  last_chunk_was_initialization_ = false;
  out->SetChunk(std::move(next_media_chunk));
}

void DashChunkSource::OnChunkLoadCompleted(chunk::Chunk* chunk) {
  if (chunk->type() == chunk::Chunk::kTypeMediaInitialization) {
    chunk::InitializationChunk* initialization_chunk =
        static_cast<chunk::InitializationChunk*>(chunk);

    const std::string& format_id = initialization_chunk->format()->GetId();
    auto find_result = period_holders_.find(initialization_chunk->parent_id());
    PeriodHolder* period_holder = (find_result == period_holders_.end())
                                      ? nullptr
                                      : find_result->second.get();

    if (!period_holder) {
      // period for this initialization chunk may no longer be on the manifest
      return;
    }

    RepresentationHolder* representation_holder =
        period_holder->GetRepresentationHolder(format_id);
    if (initialization_chunk->HasFormat()) {
      representation_holder->GiveMediaFormat(
          initialization_chunk->TakeFormat());
    }
    // The null check avoids overwriting an index obtained from the manifest
    // with one obtained from the stream. If the manifest defines an index then
    // the stream shouldn't, but in cases where it does we should ignore it.
    if (!representation_holder->segment_index() &&
        initialization_chunk->HasSeekMap()) {
      std::unique_ptr<const extractor::SeekMapInterface> seek_map =
          initialization_chunk->TakeSeekMap();
      // TODO(adewhurst): Reorganize to avoid cast
      std::unique_ptr<const extractor::ChunkIndex> chunk_index(
          static_cast<const extractor::ChunkIndex*>(seek_map.release()));

      std::unique_ptr<mpd::DashSegmentIndexInterface> segment_index(
          new DashWrappingSegmentIndex(
              std::move(chunk_index),
              initialization_chunk->data_spec()->uri.uri()));
      representation_holder->GiveSegmentIndex(std::move(segment_index));
    }
    // The null check avoids overwriting drm_init_data obtained from the
    // manifest with drm_init_data obtained from the stream, as per DASH IF
    // Interoperability Recommendations V3.0, 7.5.3.
    if (!period_holder->drm_init_data() &&
        initialization_chunk->HasDrmInitData()) {
      period_holder->SetDrmInitData(initialization_chunk->GetDrmInitData());
    }
  }
}

void DashChunkSource::OnChunkLoadError(const chunk::Chunk* chunk,
                                       chunk::ChunkLoadErrorReason e) {
  // Do nothing.
  LOG(WARNING) << "Chunk load error " << e;
  if (qoe_) {
    qoe_->ReportVideoError(qoe::VideoErrorCode::MEDIA_FETCH_ERROR,
                           "ChunkLoadError", false);
  }
}

void DashChunkSource::Disable(
    std::deque<std::unique_ptr<chunk::MediaChunk>>* queue) {
  DCHECK(track_is_enabled_);

  adaptive_format_evaluator_->Disable();
  if (manifest_fetcher_) {
    manifest_fetcher_->Disable();
  }
  period_holders_.clear();
  evaluation_.format_ = nullptr;
  available_range_.reset();
  fatal_error_ = false;
  track_is_enabled_ = false;
  track_criteria_ = nullptr;
}

namespace {

class PtrFormatBandwidthComparator {
 public:
  bool operator()(const util::Format* lhs, const util::Format* rhs) {
    return c_(*lhs, *rhs);
  }

  util::Format::DecreasingBandwidthComparator c_;
};

}  // namespace

// For TESTING
DashChunkSource::DashChunkSource(
    drm::DrmSessionManagerInterface* drm_session_manager,
    const mpd::MediaPresentationDescription* manifest,
    upstream::DataSourceInterface* data_source,
    chunk::FormatEvaluatorInterface* adaptive_format_evaluator,
    const mpd::AdaptationType& adaptation_type,
    const PlaybackRate* playback_rate,
    qoe::QoeManager* qoe)
    : DashChunkSource(
          drm_session_manager,
          nullptr,
          manifest,
          data_source,
          adaptive_format_evaluator,
          adaptation_type,
          std::unique_ptr<base::TickClock>(new base::DefaultTickClock()),
          base::TimeDelta::FromMicroseconds(0),
          base::TimeDelta::FromMicroseconds(0),
          false,
          AvailableRangeChangedCB(),
          playback_rate,
          qoe) {}

// Private constructor
DashChunkSource::DashChunkSource(
    drm::DrmSessionManagerInterface* drm_session_manager,
    ManifestFetcher* manifest_fetcher,
    const mpd::MediaPresentationDescription* initial_manifest,
    upstream::DataSourceInterface* data_source,
    chunk::FormatEvaluatorInterface* adaptive_format_evaluator,
    const mpd::AdaptationType& adaptation_type,
    std::unique_ptr<base::TickClock> clock,
    base::TimeDelta live_edge_latency,
    base::TimeDelta elapsed_realtime_offset,
    bool start_at_live_edge,
    AvailableRangeChangedCB range_changed_cb,
    const PlaybackRate* playback_rate,
    qoe::QoeManager* qoe)
    : range_changed_cb_(range_changed_cb),
      data_source_(data_source),
      adaptive_format_evaluator_(adaptive_format_evaluator),
      evaluation_(),
      adaptation_type_(adaptation_type),
      drm_session_manager_(drm_session_manager),
      manifest_fetcher_(manifest_fetcher),
      clock_(std::move(clock)),
      live_edge_latency_(live_edge_latency),
      elapsed_realtime_offset_(elapsed_realtime_offset),
      live_(initial_manifest->IsDynamic()),
      current_manifest_(initial_manifest),
      track_criteria_(nullptr),
      available_range_(new StaticTimeRange()),
      start_at_live_edge_(start_at_live_edge),
      playback_rate_(playback_rate),
      qoe_(qoe) {}

std::unique_ptr<MediaFormat> DashChunkSource::GetTrackFormat(
    mpd::AdaptationType adaptation_set_type,
    const util::Format* format,
    const std::string& media_mime_type,
    base::TimeDelta duration) {
  switch (adaptation_set_type) {
    case mpd::AdaptationType::VIDEO:
      return MediaFormat::CreateVideoFormat(
          format->GetId(), media_mime_type, format->GetCodecs(),
          format->GetBitrate(), kNoValue, duration.InMicroseconds(),
          format->GetWidth(), format->GetHeight(), nullptr, 0);
    case mpd::AdaptationType::AUDIO:
      return MediaFormat::CreateAudioFormat(
          format->GetId(), media_mime_type, format->GetCodecs(),
          format->GetBitrate(), kNoValue, duration.InMicroseconds(),
          format->GetAudioChannels(), format->GetAudioSamplingRate(), nullptr,
          0, format->GetLanguage());
    case mpd::AdaptationType::TEXT:
      return MediaFormat::CreateTextFormat(
          format->GetId(), media_mime_type, format->GetBitrate(),
          duration.InMicroseconds(), format->GetLanguage());
    default:
      return nullptr;
  }
}

bool DashChunkSource::GetMediaMimeType(const util::Format& format,
                                       std::string* out_str) {
  using util::MimeTypes;

  const std::string& format_mime_type = format.GetMimeType();
  if (MimeTypes::IsAudio(format_mime_type)) {
    *out_str = MimeTypes::GetAudioMediaMimeType(format.GetCodecs());
    return true;
  } else if (MimeTypes::IsVideo(format_mime_type)) {
    *out_str = MimeTypes::GetVideoMediaMimeType(format.GetCodecs());
    return true;
  } else if (MimeTypeIsRawText(format_mime_type)) {
    *out_str = format_mime_type;
    return true;
  } else if (format_mime_type == util::kApplicationMP4) {
    if (format.GetCodecs() == kFourccTTML) {
      *out_str = util::kApplicationTTML;
      return true;
    }
    if (format.GetCodecs() == kFourccWebVTT) {
      *out_str = util::kApplicationMP4VTT;
      return true;
    }
  }
  return false;
}

bool DashChunkSource::MimeTypeIsWebm(const std::string& mime_type) {
  return base::StartsWith(mime_type, util::kVideoWebM,
                          base::CompareCase::SENSITIVE) ||
         base::StartsWith(mime_type, util::kAudioWebM,
                          base::CompareCase::SENSITIVE) ||
         base::StartsWith(mime_type, util::kApplicationWebM,
                          base::CompareCase::SENSITIVE);
}

bool DashChunkSource::MimeTypeIsRawText(const std::string& mime_type) {
  return base::StartsWith(mime_type, util::kTextVTT,
                          base::CompareCase::SENSITIVE) ||
         base::StartsWith(mime_type, util::kApplicationTTML,
                          base::CompareCase::SENSITIVE);
}

std::unique_ptr<chunk::Chunk> DashChunkSource::NewInitializationChunk(
    const mpd::RangedUri* initialization_uri,
    const mpd::RangedUri* index_uri,
    const mpd::Representation& representation,
    chunk::ChunkExtractorWrapper* extractor,
    upstream::DataSourceInterface* data_source,
    int32_t manifest_index,
    chunk::Chunk::TriggerReason trigger,
    chunk::Chunk::FormatGivenCB format_given_cb) {
  const mpd::RangedUri* request_uri;
  std::unique_ptr<mpd::RangedUri> merged_uri;

  if (initialization_uri) {
    // It's common for initialization and index data to be stored adjacently.
    // Attempt to merge the two requests together to request both at once.
    merged_uri = initialization_uri->AttemptMerge(index_uri);
    if (merged_uri) {
      request_uri = merged_uri.get();
    } else {
      request_uri = initialization_uri;
    }
  } else {
    request_uri = index_uri;
  }
  CHECK(request_uri);
  // TODO(adewhurst): Avoid useless temporary Uri and DataSpec
  const upstream::DataSpec data_spec(
      upstream::Uri(request_uri->GetUriString()), request_uri->GetStart(),
      request_uri->GetLength(), &representation.GetCacheKey());
  std::unique_ptr<chunk::Chunk> new_chunk(new chunk::InitializationChunk(
      data_source, &data_spec, trigger, &representation.GetFormat(), extractor,
      manifest_index));
  new_chunk->SetFormatGivenCallback(format_given_cb);
  return new_chunk;
}

std::unique_ptr<chunk::Chunk> DashChunkSource::NewMediaChunk(
    const PeriodHolder& period_holder,
    RepresentationHolder* representation_holder,
    upstream::DataSourceInterface* data_source,
    const MediaFormat* media_format,
    int32_t segment_num,
    chunk::Chunk::TriggerReason trigger,
    chunk::Chunk::FormatGivenCB format_given_cb) {
  const mpd::Representation* representation =
      representation_holder->representation();
  const util::Format* format = &representation->GetFormat();
  base::TimeDelta start_time =
      representation_holder->GetSegmentStartTime(segment_num);
  base::TimeDelta end_time =
      representation_holder->GetSegmentEndTime(segment_num);
  std::unique_ptr<mpd::RangedUri> segment_uri =
      representation_holder->GetSegmentUri(segment_num);
  // TODO(adewhurst): Avoid useless temporary Uri and DataSpec
  const upstream::DataSpec data_spec(
      upstream::Uri(segment_uri->GetUriString()), segment_uri->GetStart(),
      segment_uri->GetLength(), &representation->GetCacheKey());

  base::TimeDelta sample_offset =
      period_holder.start_time() -
      base::TimeDelta::FromMicroseconds(
          representation->GetPresentationTimeOffsetUs());

  std::unique_ptr<chunk::Chunk> new_chunk;

  if (MimeTypeIsRawText(format->GetMimeType())) {
    new_chunk.reset(new chunk::SingleSampleMediaChunk(
        data_source, &data_spec, chunk::Chunk::kTriggerInitial, format,
        start_time.InMicroseconds(), end_time.InMicroseconds(), segment_num,
        media_format, nullptr, period_holder.local_index()));
    if (!format_given_cb.is_null()) {
      format_given_cb.Run(media_format);
    }
  } else {
    bool is_media_format_final = (media_format != nullptr);
    new_chunk.reset(new chunk::ContainerMediaChunk(
        data_source, &data_spec, trigger, format, start_time.InMicroseconds(),
        end_time.InMicroseconds(), segment_num, sample_offset,
        representation_holder->extractor_wrapper(), media_format,
        period_holder.drm_init_data(), is_media_format_final,
        period_holder.local_index()));
    new_chunk->SetFormatGivenCallback(format_given_cb);
  }

  return new_chunk;
}

PeriodHolder* DashChunkSource::FindPeriodHolder(base::TimeDelta position) {
  CHECK(!period_holders_.empty());

  // if position is before the first period, return the first period (as long
  // as it has representations).
  PeriodHolder* first_period_holder = period_holders_.begin()->second.get();
  size_t num_representations =
      first_period_holder->num_representation_holders();
  if (position < first_period_holder->available_start_time() &&
      num_representations > 0) {
    return first_period_holder;
  }

  for (auto ii = period_holders_.begin(); ii != period_holders_.end(); ++ii) {
    PeriodHolder* period_holder = ii->second.get();
    const base::TimeDelta* end_time = period_holder->GetAvailableEndTime();
    size_t num_representations = period_holder->num_representation_holders();
    DCHECK(end_time);
    if (position < *end_time && num_representations > 0) {
      return period_holder;
    }
  }

  return nullptr;
}
// TODO(adewhurst): Find a way to make a const version without duplicating
// everything.
const PeriodHolder* DashChunkSource::FindPeriodHolder(
    base::TimeDelta position) const {
  CHECK(!period_holders_.empty());

  // if position is before the first period, return the first period (as long
  // as it has representations).
  const PeriodHolder* first_period_holder =
      period_holders_.begin()->second.get();
  size_t num_representations =
      first_period_holder->num_representation_holders();
  if (position < first_period_holder->available_start_time() &&
      num_representations > 0) {
    return first_period_holder;
  }

  for (auto ii = period_holders_.begin(); ii != period_holders_.end(); ++ii) {
    const PeriodHolder* period_holder = ii->second.get();
    const base::TimeDelta* end_time = period_holder->GetAvailableEndTime();
    size_t num_representations = period_holder->num_representation_holders();
    DCHECK(end_time);
    if (position < *end_time && num_representations > 0) {
      return period_holder;
    }
  }

  return nullptr;
}

void DashChunkSource::ProcessManifest(
    const mpd::MediaPresentationDescription* manifest) {
  // Remove old periods.
  const mpd::Period* first_period = manifest->GetPeriod(0);
  while (period_holders_.size() > 0 &&
         period_holders_.begin()->second->start_time().InMilliseconds() <
             first_period->GetStartMs()) {
    period_holders_.erase(period_holders_.begin());
  }

  // After discarding old periods, we should never have more periods than
  // listed in the new manifest. That would mean that a previously announced
  // period is no longer advertised. If this condition occurs, assume that we
  // are hitting a manifest server that is out of sync and behind, discard this
  // manifest, and try again later.
  if (period_holders_.size() > manifest->GetPeriodCount()) {
    CHECK(manifest) << "Discarding our only manifest: manifest has fewer "
                    << "periods than period_holders_";
    return;
  }

  // Update existing periods. Only the first and last periods can change.
  int period_holder_count = period_holders_.size();
  if (period_holder_count > 0) {
    PeriodHolder* period_holder = period_holders_.begin()->second.get();
    if (!period_holder->UpdatePeriod(*manifest, 0, track_criteria_)) {
      fatal_error_ = true;
      BehindLiveWindowError(qoe_);
      CHECK(manifest) << "Discarding our only manifest: failed first period";
      return;
    }

    if (period_holder_count > 1) {
      period_holder = period_holders_.rbegin()->second.get();
      // The manifest's index corresponding to the last period in
      // period_holders_
      int32_t last_index = period_holders_.size() - 1;

      if (!period_holder->UpdatePeriod(*manifest, last_index,
                                       track_criteria_)) {
        CHECK(manifest) << "Discarding our only manifest: failed last period";
        fatal_error_ = true;
        BehindLiveWindowError(qoe_);
        return;
      }
    }
  }

  // Add new periods.
  for (int32_t i = period_holders_.size(); i < manifest->GetPeriodCount();
       i++) {
    period_holders_.insert(
        period_holders_.end(),
        std::pair<int32_t, std::unique_ptr<PeriodHolder>>(
            std::piecewise_construct,
            std::forward_as_tuple(next_period_holder_index_),
            std::forward_as_tuple(new PeriodHolder(
                drm_session_manager_, next_period_holder_index_, *manifest, i,
                track_criteria_, playback_rate_->rate()))));
    next_period_holder_index_++;
  }

  // Update the available range.
  std::unique_ptr<TimeRangeInterface> new_available_range = GetAvailableRange();
  if (!available_range_ || available_range_ != new_available_range) {
    available_range_ = std::move(new_available_range);
    NotifyAvailableRangeChanged(*available_range_);
  }

  current_manifest_ = std::move(manifest);
}

std::unique_ptr<TimeRangeInterface> DashChunkSource::GetAvailableRange() const {
  const PeriodHolder* first_period = period_holders_.begin()->second.get();
  const PeriodHolder* last_period = period_holders_.rbegin()->second.get();
  std::unique_ptr<TimeRangeInterface> out_range;

  if (!current_manifest_->IsDynamic() || last_period->index_is_explicit()) {
    out_range.reset(new StaticTimeRange(first_period->available_start_time(),
                                        *last_period->GetAvailableEndTime()));
    return out_range;
  }

  base::TimeDelta min_start_position = first_period->available_start_time();
  base::TimeDelta max_end_position = last_period->index_is_unbounded()
                                         ? base::TimeDelta::Max()
                                         : *last_period->GetAvailableEndTime();
  // TODO(adewhurst): Look into GetAvailabilityStartTime() and use something
  //                  less awkward. It currently uses "Java Time" (64-bit ms
  //                  since UNIX epoch)
  base::TimeTicks elapsed_realtime_at_zero =
      base::TimeTicks::UnixEpoch() +
      base::TimeDelta::FromMilliseconds(
          current_manifest_->GetAvailabilityStartTime());
  base::TimeDelta time_shift_buffer_depth =
      current_manifest_->GetTimeShiftBufferDepth() == -1
          ? base::TimeDelta::FromMilliseconds(0)
          : base::TimeDelta::FromMilliseconds(
                current_manifest_->GetTimeShiftBufferDepth());

  out_range.reset(new DynamicTimeRange(min_start_position, max_end_position,
                                       elapsed_realtime_at_zero,
                                       time_shift_buffer_depth, clock_.get()));
  return out_range;
}

void DashChunkSource::NotifyAvailableRangeChanged(
    const TimeRangeInterface& seek_range) {
  // TODO(adewhurst): Consider supporting posting the callback to another
  //                  thread (similar to android.os.Handler supported in
  //                  ExoPlayer)
  if (!range_changed_cb_.is_null()) {
    range_changed_cb_.Run(seek_range);
  } else {
    VLOG(1) << "Range changed; no callback";
  }
}

bool DashChunkSource::MoveToNextPeriod(PeriodHolderIterator find_result,
                                       PeriodHolder** period_holder,
                                       chunk::ChunkOperationHolder* out,
                                       int32_t previous_parent_id) {
  // We reached the end of a period. Start the next one.

  // We've moved on to the next period but we can't assume there are
  // representations for us. Keep scanning for the first period with
  // at least one representation. If not found, then we're either at
  // the end of the stream (!dynamic) or we have to set chunk null and
  // wait for more data to show up in the manifest.
  do {
    ++find_result;
    DCHECK_EQ(find_result->first, previous_parent_id + 1);
    previous_parent_id = find_result->first;
    if (find_result == period_holders_.end()) {
      if (!current_manifest_->IsDynamic()) {
        out->SetEndOfStream(true);
      }
      out->SetChunk(nullptr);
      return false;
    }
    *period_holder = find_result->second.get();
  } while ((*period_holder)->representation_holders().size() == 0);
  return true;
}

bool DashChunkSource::MoveToPrevPeriod(PeriodHolderIterator find_result,
                                       PeriodHolder** period_holder,
                                       chunk::ChunkOperationHolder* out,
                                       int32_t previous_parent_id) {
  // We reached the start of a period. Start the previous one.

  // We've moved on to the prev period but we can't assume there are
  // representations for us. Keep scanning for the first period with
  // at least one representation. If not found, then we're either at
  // the end of the stream (!dynamic) or we have to set chunk null and
  // wait for more data to show up in the manifest.
  do {
    if (find_result == period_holders_.begin()) {
      out->SetChunk(nullptr);
      return false;
    }
    --find_result;
    DCHECK_EQ(find_result->first, previous_parent_id - 1);
    previous_parent_id = find_result->first;
    *period_holder = find_result->second.get();
  } while ((*period_holder)->representation_holders().size() == 0);
  return true;
}
}  // namespace dash
}  // namespace ndash
