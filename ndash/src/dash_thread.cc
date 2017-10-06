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

#include "dash_thread.h"

#include <cinttypes>
#include <cstdint>
#include <cstring>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "chunk/adaptive_evaluator.h"
#include "chunk/chunk_sample_source.h"
#include "chunk/demo_evaluator.h"
#include "dash/dash_chunk_source.h"
#include "ndash.h"
#include "qoe/qoe_manager.h"
#include "sample_source_track_renderer.h"
#include "time_range.h"
#include "track_renderer.h"
#include "upstream/curl_data_source.h"
#include "upstream/default_allocator.h"
#include "util/mime_types.h"
#include "util/time.h"
#include "util/uri_util.h"
#include "util/util.h"

namespace ndash {

namespace {
const base::TimeDelta kMaxWaitCodecTime = base::TimeDelta::FromSeconds(6);
const size_t kVideoBufSize = 5242880;  // 5 MB
const size_t kAudioBufSize = 2097152;  // 2 MB
const size_t kTextBufSize = 1572864;   // 1.5 MB
const base::TimeDelta kUpdateScheduleDelay =
    base::TimeDelta::FromMilliseconds(400);
const base::TimeDelta kTrackSummaryDelay = base::TimeDelta::FromSeconds(5);
const base::TimeDelta kBandwidthEstimateDelay = base::TimeDelta::FromSeconds(5);
const char kCurlGlobalLock[] = "curl-global-lock";
const char kNoCurlGlobalLock[] = "no-curl-global-lock";
const char kAllTracksMetered[] = "all-tracks-metered";
const char kNoAllTracksMetered[] = "no-all-tracks-metered";

void __attribute__((unused))
AvailableRangeChanged(const char* track,
                      const TimeRangeInterface& available_range) {
  TimeRangeInterface::TimeDeltaPair range = available_range.GetCurrentBounds();
  LOG(INFO) << __FUNCTION__ << " / " << track
            << " static=" << available_range.IsStatic() << " range=["
            << range.first << " - " << range.second << "]";
}

template <typename ReturnType>
void CallThenSignal(ReturnType* ret,
                    const base::Callback<ReturnType()>& task,
                    base::WaitableEvent* done) {
  *ret = task.Run();
  done->Signal();
}

template <typename ReturnType>
void APICallAndWait(DashThread* dash,
                    ReturnType* ret,
                    const tracked_objects::Location& from_here,
                    const base::Callback<ReturnType()>& task) {
  base::WaitableEvent done(true, false);

  dash->task_runner()->PostTask(
      from_here, base::Bind(&CallThenSignal<ReturnType>, ret, task, &done));

  done.Wait();
}

}  // namespace

DashThread::TrackContext::TrackContext() : sample_holder_(true) {}
DashThread::TrackContext::~TrackContext() {}

DashThread::DashThread(const std::string& name, void* context)
    : Thread(name),
      context_(context),
      state_(STATE_IDLE),
      current_track_(nullptr),
      sample_holder_consumed_(0),
      is_eos_(false),
      media_time_ready_(false),
      decoder_media_time_last_value_ms_(0),
      media_time_last_value_ms_(0),
      player_callbacks_({0}),
      license_fetcher_(
          base::WrapUnique(new upstream::CurlDataSource("license"))),
      drm_session_manager_(&context_, &player_callbacks_),
      qoe_manager_(nullptr),
      tracks_(),
      unload_waiter_(false, false),
      codec_waiter_(true, false),
      playback_rate_waiter_(false, false),
      sample_offset_ms_(-1) {
  LOG(INFO) << "DashThread";
}

DashThread::~DashThread() {
  LOG(INFO) << "~DashThread";
  Unload();
  Stop();
}

bool DashThread::Load(const char* url, int32_t initial_time_sec) {
  DCHECK(initial_time_sec >= 0);
  LOG(INFO) << "load";

  initial_time_ = base::TimeDelta::FromSeconds(initial_time_sec);

  url_ = std::string(url);

  qoe_manager_.reset(new qoe::QoeManager());

  qoe_manager_->SetMediaPos(initial_time_);
  qoe_manager_->ReportPreparing();

  // Post the initial update task to kick things off.
  task_runner()->PostTask(FROM_HERE, base::Bind(&DashThread::Update,
                                                base::Unretained(this), false));

  // Do not let caller proceed until we know both video and audio codecs
  // for GetVideoCodecSettings and GetAudioCodecSettings functions below.
  // We do our best to signal this waiter on error conditions that could
  // prevent it from being signaled. However, to guarantee we return, we give a
  // a reasonable time before giving up and reporting the load failed.
  codec_waiter_.TimedWait(kMaxWaitCodecTime);

  if (!HaveCodecs()) {
    LOG(ERROR) << "Failed to obtain codecs from stream";
    return false;
  }
  return true;
}

void DashThread::Unload() {
  // Unload must be performed on the DashThread's task runner. We block until
  // the unload has completed after which it is safe to destroy this instance.
  task_runner()->PostTask(
      FROM_HERE, base::Bind(&DashThread::UnloadImpl, base::Unretained(this)));
  unload_waiter_.Wait();

  tracks_.clear();
  load_control_.reset();
  allocator_.reset();
  manifest_fetcher_.reset();
}

void DashThread::SetPlayerCallbacks(const DashPlayerCallbacks* callbacks) {
  player_callbacks_ = *callbacks;
}

void DashThread::SetPlayerCallbackContext(void* context) {
  context_ = context;
}

bool DashThread::SetAttribute(const std::string& attr_name,
                              const std::string& attr_value) {
  if (attr_name == "auth") {
    player_attributes_.auth_token = attr_value;
    license_fetcher_.UpdateAuthToken(attr_value);
    return true;
  } else if (attr_name == "license_url") {
    player_attributes_.license_url = attr_value;
    license_fetcher_.UpdateLicenseUri(upstream::Uri(attr_value));
    return true;
  }
  LOG(WARNING) << "Unknown attribute " << attr_name;
  return false;
}

void DashThread::SetPlaybackRate(float target_rate) {
  LOG(INFO) << "SetPlaybackRate " << target_rate;
  playback_rate_waiter_.Reset();
  task_runner()->PostTask(FROM_HERE,
                          base::Bind(&DashThread::SetPlaybackRateDisableTracks,
                                     base::Unretained(this), target_rate));
  playback_rate_waiter_.Wait();
}

// Manifest EventListenerInterface callback implementation.

void DashThread::OnManifestRefreshStarted() {}

void DashThread::OnManifestRefreshed() {
  if (state_ != STATE_PREPARING) {
    return;
  }

  qoe_manager_->ReportInitializingStream();

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  bool curl_global_lock = false;
  if (command_line && command_line->HasSwitch(kCurlGlobalLock)) {
    curl_global_lock = true;
  } else if (command_line && command_line->HasSwitch(kNoCurlGlobalLock)) {
    curl_global_lock = false;
  }

  bool all_tracks_metered = true;
  if (command_line && command_line->HasSwitch(kAllTracksMetered)) {
    all_tracks_metered = true;
  } else if (command_line && command_line->HasSwitch(kNoAllTracksMetered)) {
    all_tracks_metered = false;
  }

  // Set up video
  tracks_.emplace_back();
  TrackContext& video_track = tracks_.back();
  video_track.name_ = "video";
  video_track.frame_type_ = DASH_FRAME_TYPE_VIDEO;
  video_track.data_source_.reset(new upstream::CurlDataSource(
      "video", media_bandwidth_meter_.get(), curl_global_lock));
  video_track.format_evaluator_.reset(
      new chunk::AdaptiveEvaluator(media_bandwidth_meter_.get()));

  // Url goes into dash chunk source here...
  video_track.chunk_source_.reset(new dash::DashChunkSource(
      &drm_session_manager_, manifest_fetcher_.get(),
      video_track.data_source_.get(), video_track.format_evaluator_.get(),
      mpd::AdaptationType::VIDEO, base::TimeDelta::FromSeconds(1),
      base::TimeDelta(), false, base::Bind(AvailableRangeChanged, "video"),
      &playback_rate_, qoe_manager_.get()));
  video_track.chunk_source_->SetFormatGivenCallback(
      base::Bind(&DashThread::FormatGiven, base::Unretained(this),
                 base::Unretained(&video_track)));
  video_track.sample_source_.reset(new chunk::ChunkSampleSource(
      video_track.chunk_source_.get(), load_control_.get(), &playback_rate_,
      kVideoBufSize, this));
  video_track.renderer_.reset(
      new SampleSourceTrackRenderer(video_track.sample_source_.get()));
  video_track.track_criteria_.reset(new TrackCriteria("video/*"));

  // Set up audio
  tracks_.emplace_back();
  TrackContext& audio_track = tracks_.back();
  audio_track.name_ = "audio";
  audio_track.frame_type_ = DASH_FRAME_TYPE_AUDIO;
  audio_track.data_source_.reset(new upstream::CurlDataSource(
      "audio", all_tracks_metered ? media_bandwidth_meter_.get() : nullptr,
      curl_global_lock));
  audio_track.format_evaluator_.reset(new chunk::DemoEvaluator());

  // Url goes into dash chunk source here...
  audio_track.chunk_source_.reset(new dash::DashChunkSource(
      &drm_session_manager_, manifest_fetcher_.get(),
      audio_track.data_source_.get(), audio_track.format_evaluator_.get(),
      mpd::AdaptationType::AUDIO, base::TimeDelta::FromSeconds(1),
      base::TimeDelta(), false, base::Bind(AvailableRangeChanged, "audio"),
      &playback_rate_, qoe_manager_.get()));
  audio_track.chunk_source_->SetFormatGivenCallback(
      base::Bind(&DashThread::FormatGiven, base::Unretained(this),
                 base::Unretained(&audio_track)));
  audio_track.sample_source_.reset(new chunk::ChunkSampleSource(
      audio_track.chunk_source_.get(), load_control_.get(), &playback_rate_,
      kAudioBufSize, this));
  audio_track.renderer_.reset(
      new SampleSourceTrackRenderer(audio_track.sample_source_.get()));

  audio_track.track_criteria_.reset(new TrackCriteria("audio/*"));

  // We always prefer e-ac3 for now.
  audio_track.track_criteria_->preferred_codec = kAudioCodecEAC3;

  // Set up text
  tracks_.emplace_back();
  TrackContext& text_track = tracks_.back();
  text_track.name_ = "text";
  text_track.frame_type_ = DASH_FRAME_TYPE_CC;
  text_track.data_source_.reset(new upstream::CurlDataSource(
      "text", all_tracks_metered ? media_bandwidth_meter_.get() : nullptr,
      curl_global_lock));
  text_track.format_evaluator_.reset(new chunk::DemoEvaluator());

  // Url goes into dash chunk source here...
  text_track.chunk_source_.reset(new dash::DashChunkSource(
      &drm_session_manager_, manifest_fetcher_.get(),
      text_track.data_source_.get(), text_track.format_evaluator_.get(),
      mpd::AdaptationType::TEXT, base::TimeDelta::FromSeconds(1),
      base::TimeDelta(), false, base::Bind(AvailableRangeChanged, "text"),
      &playback_rate_, qoe_manager_.get()));
  text_track.sample_source_.reset(new chunk::ChunkSampleSource(
      text_track.chunk_source_.get(), load_control_.get(), &playback_rate_,
      kTextBufSize));
  text_track.renderer_.reset(
      new SampleSourceTrackRenderer(text_track.sample_source_.get()));
  text_track.track_criteria_.reset(new TrackCriteria(util::kApplicationRAWCC));

  for (auto& track : tracks_) {
    int state = track.renderer_->Prepare(initial_time_.InMicroseconds());

    if (state != TrackRenderer::PREPARED) {
      LOG(ERROR) << track.name_ << " could not prepare for initial time "
                 << initial_time_;
      SetState(STATE_ENDED);
      return;
    }

    if (!track.renderer_->Enable(track.track_criteria_.get(),
                                 initial_time_.InMicroseconds(), false)) {
      LOG(ERROR) << "Problem enabling " << track.name_ << " renderer";
      SetState(STATE_ENDED);
      return;
    }

    if (!track.renderer_->Start()) {
      LOG(ERROR) << "Problem starting " << track.name_ << " renderer";
      SetState(STATE_ENDED);
      return;
    }
  }

  SetState(STATE_BUFFERING);
  qoe_manager_->ReportBuffering();

  duration_ = base::TimeDelta::FromMilliseconds(
      manifest_fetcher_->GetManifest()->GetDuration());

  Update(true);
}

void DashThread::OnManifestError(ManifestFetcher::ManifestFetchError error) {
  std::string detail;
  switch (error) {
    case ManifestFetcher::ManifestFetchError::PARSING_ERROR:
      detail = "ParsingError";
      break;
    case ManifestFetcher::ManifestFetchError::NETWORK_ERROR:
      detail = "NetworkError";
      break;
    default:
      detail = "UnknownError";
      break;
  }

  ReportPlaybackError(qoe::VideoErrorCode::MANIFEST_FETCH_ERROR, detail, false);

  if (state_ == STATE_PREPARING) {
    SetState(STATE_ENDED);
  }
}

void DashThread::SetState(PlayerState new_state) {
  state_ = new_state;
  if (state_ == STATE_ENDED) {
    codec_waiter_.Signal();
  }
}

void DashThread::OnLoadStarted(int32_t source_id,
                               int64_t length,
                               int32_t type,
                               int32_t trigger,
                               const util::Format* format,
                               int64_t media_start_time_ms,
                               int64_t media_end_time_ms) {
  // Nothing to do.
}

void DashThread::OnLoadCompleted(int32_t source_id,
                                 int64_t bytes_loaded,
                                 int32_t type,
                                 int32_t trigger,
                                 const util::Format* format,
                                 int64_t media_start_time_ms,
                                 int64_t media_end_time_ms,
                                 base::TimeTicks elapsed_real_time,
                                 base::TimeDelta load_duration) {
  if (type == chunk::Chunk::kTypeMedia) {
    qoe::LoadType load_type;
    std::string mime_type = format->GetMimeType();
    if (util::MimeTypes::IsVideo(mime_type)) {
      load_type = qoe::LoadType::VIDEO;
    } else if (util::MimeTypes::IsAudio(mime_type)) {
      load_type = qoe::LoadType::AUDIO;
    } else if (util::MimeTypes::IsText(mime_type)) {
      load_type = qoe::LoadType::CLOSED_CAPTIONS;
    } else {
      LOG(WARNING) << "Unhandled mime type in OnLoadCompleted callback - "
                   << mime_type;
      load_type = qoe::LoadType::UNKNOWN;
    }

    int64_t load_end_ms = elapsed_real_time.ToInternalValue() /
                          base::TimeTicks::kMicrosecondsPerMillisecond;
    int64_t load_start_ms = load_end_ms - load_duration.InMilliseconds();

    qoe_manager_->ReportContentLoad(load_type, media_start_time_ms,
                                    media_end_time_ms,
                                    load_duration.InMilliseconds(),
                                    bytes_loaded, load_start_ms, load_end_ms);
  } else {
    LOG(WARNING) << "Unhandled chunk type in OnLoadCompleted callback";
  }
}

void DashThread::OnLoadCanceled(int32_t source_id, int64_t bytesLoaded) {
  // Nothing to do.
}

void DashThread::OnLoadError(int32_t source_id, chunk::ChunkLoadErrorReason e) {
  qoe_manager_->ReportVideoError(qoe::VideoErrorCode::MEDIA_FETCH_ERROR,
                                 "OnLoadError", false);
}

void DashThread::OnUpstreamDiscarded(int32_t source_id,
                                     int64_t media_start_time_ms,
                                     int64_t media_end_time_ms) {
  // Nothing to do.
}

void DashThread::OnDownstreamFormatChanged(int32_t source_id,
                                           const util::Format* format,
                                           int32_t trigger,
                                           int64_t media_time_ms) {
  // Nothing to do.
}

// Static members for delegating C API callbacks

int DashThread::GetVideoCodecSettings(DashThread* dash,
                                      DashVideoCodecSettings* codec_settings) {
  int ret;
  auto cb = base::Bind(&DashThread::GetVideoCodecSettingsImpl,
                       base::Unretained(dash), codec_settings);
  APICallAndWait(dash, &ret, FROM_HERE, cb);
  return ret;
}

int DashThread::GetAudioCodecSettings(DashThread* dash,
                                      DashAudioCodecSettings* codec_settings) {
  int ret;
  auto cb = base::Bind(&DashThread::GetAudioCodecSettingsImpl,
                       base::Unretained(dash), codec_settings);
  APICallAndWait(dash, &ret, FROM_HERE, cb);
  return ret;
}

int DashThread::CopyFrame(DashThread* dash,
                          void* buffer,
                          int bufferlen,
                          struct DashFrameInfo* fi) {
  int ret;
  auto cb = base::Bind(&DashThread::CopyFrameImpl, base::Unretained(dash),
                       buffer, bufferlen, fi);
  APICallAndWait(dash, &ret, FROM_HERE, cb);

  if (ret == -1) {
    // Nothing to do, slow down the pull reader poll
    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(50));
    ret = 0;
  }

  return ret;
}

int DashThread::GetCCCodecSettings(DashThread* dash,
                                   DashCCCodecSettings* settings) {
  int ret;
  auto cb = base::Bind(&DashThread::GetCCCodecSettingsImpl,
                       base::Unretained(dash), settings);
  APICallAndWait(dash, &ret, FROM_HERE, cb);
  return ret;
}

MediaTimeMs DashThread::GetFirstTime(DashThread* dash) {
  MediaTimeMs ret;
  auto cb = base::Bind(&DashThread::GetFirstTimeMsImpl, base::Unretained(dash));
  APICallAndWait(dash, &ret, FROM_HERE, cb);
  return ret;
}

MediaDurationMs DashThread::GetDurationMs(DashThread* dash) {
  MediaDurationMs ret;
  auto cb = base::Bind(&DashThread::GetDurationMsImpl, base::Unretained(dash));
  APICallAndWait(dash, &ret, FROM_HERE, cb);
  return ret;
}

// static
int DashThread::Seek(DashThread* dash, MediaTimeMs time) {
  int ret;
  auto cb = base::Bind(&DashThread::SeekImpl, base::Unretained(dash), time);
  APICallAndWait(dash, &ret, FROM_HERE, cb);
  return ret;
}

// TODO(rdaum): Expose this in the C API.
int DashThread::GetStreamCounts(DashThread* dash,
                                int* num_videostreams,
                                int* num_audiostreams,
                                int* num_cc_streams) {
  int ret;
  auto cb = base::Bind(&DashThread::GetStreamCountsImpl, base::Unretained(dash),
                       num_videostreams, num_audiostreams, num_cc_streams);
  APICallAndWait(dash, &ret, FROM_HERE, cb);
  return ret;
}

// Begin private methods

void DashThread::UnloadImpl() {
  LOG(INFO) << "DashThread::UnloadImpl";
  SetState(STATE_ENDED);
  TrackRenderer::RendererState state;
  // Transition renderer states back until they are released.
  pending_disable_.clear();
  for (auto& track : tracks_) {
    state = track.renderer_->GetState();
    if (state == TrackRenderer::STARTED) {
      track.renderer_->Stop();
    }
    state = track.renderer_->GetState();
    if (state == TrackRenderer::ENABLED) {
      pending_disable_.insert(track.renderer_.get());
    }
  }

  qoe_manager_->ReportVideoStopped();

  if (pending_disable_.empty()) {
    LOG(INFO) << "No tracks to disable";
    unload_waiter_.Signal();
    return;
  }

  for (TrackRenderer* renderer : pending_disable_) {
    base::Closure callback =
        base::Bind(&DashThread::UnloadImplDisabled, base::Unretained(this),
                   base::Unretained(renderer));
    renderer->Disable(&callback);
  }
}

void DashThread::UnloadImplDisabled(TrackRenderer* disabled_renderer) {
  auto i = pending_disable_.find(disabled_renderer);
  DCHECK(i != pending_disable_.end());
  pending_disable_.erase(disabled_renderer);

  if (pending_disable_.size() > 0) {
    // Still waiting for more renderers to be disabled.
    return;
  }

  TrackRenderer::RendererState state;
  for (auto& track : tracks_) {
    state = track.renderer_->GetState();
    if (state != TrackRenderer::RELEASED) {
      track.renderer_->Release();
    }
  }

  task_runner()->PostTask(FROM_HERE, base::Bind(&DashThread::NotifyUnloadWaiter,
                                                base::Unretained(this)));
}

void DashThread::NotifyUnloadWaiter() {
  unload_waiter_.Signal();
}

void DashThread::ScheduleUpdate() {
  task_runner()->PostDelayedTask(
      FROM_HERE, base::Bind(&DashThread::Update, base::Unretained(this), true),
      kUpdateScheduleDelay);
}

void DashThread::Update(bool allow_schedule) {
  UpdateMediaTime();

  if (state_ == STATE_IDLE) {
    SetState(STATE_PREPARING);

    // !! TODO(rmrossi): Make allocator size configurable.
    allocator_ = std::unique_ptr<upstream::AllocatorInterface>(
        new upstream::DefaultAllocator(32768, 192));
    load_control_ =
        std::unique_ptr<LoadControl>(new LoadControl(allocator_.get()));

    manifest_fetcher_ = std::unique_ptr<ManifestFetcher>(
        new ManifestFetcher(url_, task_runner(), this));

    // When refresh is done, our state will move to buffering.
    manifest_fetcher_->RequestRefresh();
    qoe_manager_->ReportLoadingManifest();

    media_bandwidth_meter_.reset(new upstream::DefaultBandwidthMeter(
        base::Bind(&DashThread::NewBandwidthEstimate, base::Unretained(this)),
        task_runner()));
  } else if (state_ == STATE_BUFFERING) {
    LOG(INFO) << "update state=" << state_ << " dpos=" << decoder_position_
              << " rpos=" << reader_position_
              << " buffer=" << buffered_position_;

    // Advance the player state.
    for (auto& track : tracks_) {
      if (track.renderer_->GetState() == TrackRenderer::STARTED ||
          track.renderer_->GetState() == TrackRenderer::ENABLED) {
        track.renderer_->Buffer(reader_position_.InMicroseconds());
      }
    }

    if (qoe_manager_) {
      qoe_manager_->ReportUpdate();
    }

    if (allow_schedule) {
      ScheduleUpdate();
    }
  } else if (state_ == STATE_READY) {
    LOG(INFO) << "update state=" << state_;
    if (allow_schedule) {
      ScheduleUpdate();
    }
  } else {
    LOG(INFO) << "update state=" << state_;
  }
}

void DashThread::UpdateMediaTime() {
  if (player_callbacks_.get_media_time_ms_func == nullptr) {
    LOG(ERROR) << "get_media_time_ms_func needs to be set";
    return;
  }

  bool decoder_time_moved = false;
  base::TimeTicks now = base::TimeTicks::Now();
  if (now >= decoder_media_time_last_call_timestamp_ +
                 base::TimeDelta::FromMilliseconds(1000)) {
    int64_t old_decoder_time_ms = decoder_media_time_last_value_ms_;

    int64_t new_decoder_time_ms =
        player_callbacks_.get_media_time_ms_func(context_);
    if (new_decoder_time_ms == -1) {
      return;
    }

    decoder_media_time_last_value_ms_ = new_decoder_time_ms;
    decoder_media_time_last_call_timestamp_ = now;

    if (old_decoder_time_ms != decoder_media_time_last_value_ms_) {
      decoder_time_moved = true;
    }
  }

  if (decoder_time_moved) {
    media_time_last_value_ms_ =
        decoder_media_time_last_value_ms_ + GetSampleOffsetMs();
    media_time_ready_ = true;
    decoder_position_ =
        base::TimeDelta::FromMilliseconds(media_time_last_value_ms_);
    if (qoe_manager_) {
      qoe_manager_->SetMediaPos(decoder_position_);
    }
  }
}

int64_t DashThread::GetSampleOffsetMs() {
  if (sample_offset_ms_ != -1) {
    return sample_offset_ms_;
  }

  // sample_offset_ will be the value that will shift the master timeline
  // so that the first frame we deliver to the client will appear at PTS 0
  sample_offset_ms_ = 0;
  if (manifest_fetcher_->GetManifest()->GetPeriodCount() > 0) {
    const mpd::Period* first_period =
        manifest_fetcher_->GetManifest()->GetPeriod(0);
    int64_t start_time_ms = first_period->GetStartMs();
    int64_t pto_us = 0;
    // Either video/audio should be sufficient to figure out the offset. Try
    // audio first.
    const mpd::AdaptationSet* adaptation = nullptr;
    int index = first_period->GetAdaptationSetIndex(mpd::AdaptationType::AUDIO);
    if (index >= 0) {
      adaptation = first_period->GetAdaptationSet(index);
    }
    if (adaptation == nullptr) {
      index = first_period->GetAdaptationSetIndex(mpd::AdaptationType::VIDEO);
      adaptation = first_period->GetAdaptationSet(index);
    }
    if (adaptation != nullptr && adaptation->NumRepresentations() > 0) {
      const mpd::Representation* rep = adaptation->GetRepresentation(0);
      pto_us = rep->GetPresentationTimeOffsetUs();
    }
    sample_offset_ms_ =
        start_time_ms -
        base::TimeDelta::FromMicroseconds(pto_us).InMilliseconds();
  }
  return sample_offset_ms_;
}

int DashThread::ReadFrame(TrackRenderer* track_renderer,
                          MediaFormatHolder* format_holder,
                          SampleHolder* sample_holder,
                          bool* error_occurred) {
  *error_occurred = false;

  DCHECK(track_renderer);
  if (track_renderer->GetState() != TrackRenderer::STARTED &&
      track_renderer->GetState() != TrackRenderer::ENABLED) {
    return SampleSourceReaderInterface::NOTHING_READ;
  }

  int result =
      track_renderer->ReadFrame(reader_position_.InMicroseconds(),
                                format_holder, sample_holder, error_occurred);
  // TODO(rmrossi): I don't think we need to keep track of buffered_position
  // unless we need to show this info to the user for some reason.  Consider
  // getting rid of this block.
  if (buffered_position_.is_zero()) {
    // We've already encountered a track for which the buffered position is
    // unknown. Hence the media buffer position unknown regardless of the
    // buffered position of this track.
  } else {
    int64_t renderer_duration_us = track_renderer->GetDurationUs();
    int64_t renderer_buffered_position_us =
        track_renderer->GetBufferedPositionUs();
    if (renderer_buffered_position_us == util::kUnknownTimeUs) {
      buffered_position_ = base::TimeDelta();
    } else if (renderer_buffered_position_us == util::kEndOfTrackUs ||
               (renderer_duration_us != util::kUnknownTimeUs &&
                renderer_duration_us != util::kMatchLongestUs &&
                renderer_buffered_position_us >= renderer_duration_us)) {
      // This track is fully buffered.
    } else {
      buffered_position_ = std::min(
          buffered_position_,
          base::TimeDelta::FromMicroseconds(renderer_buffered_position_us));
    }
  }

  return result;
}

// Private implementation functions exposed through the C API.

int DashThread::GetVideoCodecSettingsImpl(DashVideoCodecSettings* settings) {
  for (auto& track : tracks_) {
    if (track.frame_type_ == DASH_FRAME_TYPE_VIDEO) {
      // We should not have been allowed to get here unless we have the
      // upstream media format. See Load().
      CHECK(track.upstream_format_.get() != nullptr);
      settings->width = track.upstream_format_->GetWidth();
      settings->height = track.upstream_format_->GetHeight();
      const std::string& video_codec = track.upstream_format_->GetCodecs();
      LOG(INFO) << "Detected video codec " << video_codec;
      if (video_codec == kVideoCodecH264) {
        settings->video_codec = DASH_VIDEO_H264;
      } else {
        settings->video_codec = DASH_VIDEO_UNSUPPORTED;
      }
      return 0;
    }
  }

  LOG(ERROR) << "Did not find video codec in mp4 stream";
  return -1;
}

int DashThread::GetAudioCodecSettingsImpl(DashAudioCodecSettings* settings) {
  for (auto& track : tracks_) {
    if (track.frame_type_ == DASH_FRAME_TYPE_AUDIO) {
      // We should not have been allowed to get here unless we have the
      // upstream media format. See Load().
      CHECK(track.upstream_format_ != nullptr);
      settings->num_channels = track.upstream_format_->GetChannelCount();
      // NOTE: bps not required for these codecs.
      settings->bps = 0;
      settings->bitrate = track.upstream_format_->GetBitrate();
      settings->sample_rate = track.upstream_format_->GetSampleRate();
      settings->blockalign = 0;
      settings->sample_format = track.upstream_format_->GetSampleFormat();
      settings->channel_layout = track.upstream_format_->GetChannelLayout();
      const std::string& audio_codec = track.upstream_format_->GetCodecs();
      LOG(INFO) << "Detected audio codec " << audio_codec;
      if (audio_codec == kAudioCodecAAC) {
        settings->audio_codec = DASH_AUDIO_AAC;
      } else if (audio_codec == kAudioCodecAC3) {
        settings->audio_codec = DASH_AUDIO_AC3;
      } else if (audio_codec == kAudioCodecEAC3) {
        settings->audio_codec = DASH_AUDIO_EAC3;
      } else {
        settings->audio_codec = DASH_AUDIO_UNSUPPORTED;
      }
      return 0;
    }
  }

  LOG(ERROR) << "Did not find audio codec in mp4 stream";
  return -1;
}

// May be called from any thread (e.g. from media thread, or from dash
// thread inside a UpdateMediaTime() call)
bool DashThread::IsEOS() const {
  return is_eos_;
}

bool DashThread::FillTrackSampleHolders() {
  constexpr int kMaxReadAttempts = 5;

  // Detect when all tracks are EOF
  bool all_eos = true;

  // Try to ensure that all tracks have a sample ready
  for (TrackContext& track : tracks_) {
    if (track.is_eos_)
      continue;

    TrackRenderer::RendererState state = track.renderer_->GetState();

    // Don't consider a track unless its started or enabled.
    if (state != TrackRenderer::RendererState::STARTED &&
        state != TrackRenderer::RendererState::ENABLED)
      continue;

    if (track.has_sample_) {
      all_eos = false;
      continue;
    }

    track.sample_holder_.ClearData();

    bool error_occurred = false;

    for (int i = 0; i < kMaxReadAttempts; i++) {
      int result = ReadFrame(track.renderer_.get(), &track.format_holder_,
                             &track.sample_holder_, &error_occurred);

      if (result == SampleSourceReaderInterface::FORMAT_READ) {
        // TODO(rmrossi): Add another callback to decoder callbacks to inform
        // it of new format information.

        // Pssh may have changed for this track.  Make sure to call Join() below
        // on the drm session manager to ensure we have a playback license.
        track.check_pssh_ = true;
        VLOG(4) << track.name_ << " no frame read: FORMAT_READ signalled";
        continue;
      }

      if (result == SampleSourceReaderInterface::END_OF_STREAM) {
        track.is_eos_ = true;

        VLOG(2) << track.name_ << " no frame read: END_OF_STREAM signalled";
        break;
      }

      if (result == SampleSourceReaderInterface::NOTHING_READ) {
        VLOG(3) << track.name_ << " no frame read: not ready";
        break;
      }

      if (error_occurred ||
          result != SampleSourceReaderInterface::SAMPLE_READ ||
          track.sample_holder_.GetWrittenSize() == 0) {
        VLOG(2) << track.name_ << " no frame read: failed read";
        track.sample_holder_.ClearData();
        continue;
      }

      // Throw away audio/text frames until video is present. Seek times are
      // aligned to video segment boundaries, so we are guaranteed a video key
      // frame immediately. The audio and text segments do not necessarily
      // align with the video segments, so there may be samples with a
      // timestamp earlier than the target seek time. We can recognize these by
      // checking if the samples are marked "decoder only". Assuming that all
      // audio/text samples are key frames, we don't have to be very careful
      // about throwing the samples away.
      //
      if ((track.sample_holder_.GetFlags() & util::kSampleFlagDecodeOnly) !=
              0 &&
          (track.frame_type_ != DASH_FRAME_TYPE_VIDEO ||
           playback_rate_.IsTrick())) {
        if ((track.sample_holder_.GetFlags() & util::kSampleFlagSync) == 0) {
          LOG(WARNING) << "Attempting to throw away non-keyframe on non-video "
                       << "track. Update the seek logic to handle this!";
          // We have a sample that is non-video but has samples that are not
          // key frames. To handle this, there will need to be logic to pass
          // through everything from the last key frame until the target seek
          // time. Supporting decode-only samples is probably the best path
          // forward here.
          break;
        }

        VLOG(6) << track.name_ << " throwing away decode only frame";
        track.sample_holder_.ClearData();
        i--;
        continue;
      }

      VLOG(6) << track.name_ << " sample read; size "
              << track.sample_holder_.GetWrittenSize();
      track.has_sample_ = true;
      break;
    }

    all_eos = false;
  }

  return !all_eos;
}

DashThread::TrackContext* DashThread::GetNextSampleTrack() {
  // This is the maximum amount of time that mcnmp_server is allowed to buffer.
  // We don't want to move too much data into that buffer, otherwise a seek
  // forward will jump before our sample queue, which will trigger a full
  // rebuffer rather than a quick discard of all of the preceding samples.
  static const base::TimeDelta kMaxPRBuffer = base::TimeDelta::FromSeconds(5);

  TrackContext* selected_track = nullptr;
  base::TimeDelta selected_track_time;

  // media_time_track points to the track the decoder uses to determine
  // the current media time. When both audio and video tracks are available,
  // the audio track defines the media time. Otherwise, the decoder falls
  // back to the video track.
  const TrackContext* media_time_track = nullptr;

  // Find the next frame. It could be from any of the tracks, so find the one
  // with the lowest timestamp on its next frame.
  for (TrackContext& track : tracks_) {
    // Skip tracks that have no data available
    if (track.is_eos_ || !track.has_sample_)
      continue;

    if (track.frame_type_ == DASH_FRAME_TYPE_AUDIO) {
      media_time_track = &track;
    } else if (track.frame_type_ == DASH_FRAME_TYPE_VIDEO &&
               media_time_track == nullptr) {
      media_time_track = &track;
    }

    // Don't select the text track unless media time is ready. This is only
    // an issue for rawcc since there is no chunking.  The whole text track is
    // downloaded and parsed which means all samples are immediately available
    // in the sample queue. If we allow text to be considered here while media
    // time is not ready, we will allow all the data to be slurped up by the
    // player since the kMaxPRBuffer limit depends on a valid media time.
    // This condition should be harmless for other types of text tracks that
    // would behave better (i.e. webvtt) if we eventually do support them.
    // Their sample availability would be natually limited to the current
    // playback position due to load control logic anyway.
    if (track.frame_type_ == DASH_FRAME_TYPE_CC && !media_time_ready_) {
      continue;
    }

    base::TimeDelta sample_time =
        base::TimeDelta::FromMicroseconds(track.sample_holder_.GetTimeUs());
    VLOG(5) << "Candidate track " << track.name_
            << " next sample: " << sample_time;

    // When moving forward, pick lowest sample time, when moving backwards
    // pick highest sample time.
    bool is_best_by_time = playback_rate_.IsForward()
                               ? sample_time < selected_track_time
                               : sample_time > selected_track_time;
    base::TimeDelta diff = sample_time - decoder_position_;
    bool is_not_too_far_ahead =
        playback_rate_.IsForward()
            ? diff <= kMaxPRBuffer * playback_rate_.rate()
            : diff >= kMaxPRBuffer * playback_rate_.rate();
    // NOTE: PTS rollover is not a concern here since the timestamps are
    // from the master timeline, not per period.
    if ((!selected_track || is_best_by_time) &&
        (!media_time_ready_ || is_not_too_far_ahead)) {
      selected_track = &track;
      selected_track_time = sample_time;
    }
  }

  if (!selected_track) {
    VLOG(3) << "No track selected";
    return nullptr;
  }

  selected_track->times_selected_++;

  VLOG(3) << "Selected track " << selected_track->name_ << " (sample time "
          << selected_track_time << ")";

  // Beginning of iteration loop.

  // If duration_.is_zero(), we don't have a known duration (e.g. live), so
  // just use the max TimeDelta value as a placeholder to ensure we keep
  // buffering if we can.
  buffered_position_ = duration_.is_zero() ? base::TimeDelta::Max() : duration_;
  elapsed_real_time_ = base::TimeTicks::Now();

  // Periodic log message reporting on relative numbers of samples from each
  // track
  if (LOG_IS_ON(INFO) &&
      elapsed_real_time_ - last_track_summary_ > kTrackSummaryDelay) {
    logging::LogMessage log(__FILE__, __LINE__, logging::LOG_INFO);
    log.stream() << "Track selections:";
    for (TrackContext& track : tracks_) {
      log.stream() << " " << track.name_ << " " << track.times_selected_;
      track.times_selected_ = 0;
    }
    last_track_summary_ = elapsed_real_time_;
  }

  if (selected_track == media_time_track) {
    reader_position_ = base::TimeDelta::FromMicroseconds(
        selected_track->sample_holder_.GetTimeUs());
  }

  return selected_track;
}

bool DashThread::MaybeCheckPssh(TrackContext* track) {
  // We must ensure at this point we have a playback license for the key id
  // we are about to return OR we are waiting for a response to a
  // request we already made (it's too late to make the request now).  If we
  // already have a license, we're good.  If a response is pending, we
  // can wait for a bit (approx the number of seconds the decoder's buffer
  // holds but no more).
  if (!track->has_sample_ || !track->sample_holder_.IsEncrypted()) {
    // No sample or not encrypted, so nothing to check. In case an encrypted
    // sample appears later, don't reset check_pssh_.
    return true;
  }

  if (track->check_pssh_) {
    const drm::DrmInitDataInterface* drm_init_data =
        track->format_holder_.drm_init_data.get();
    DCHECK(drm_init_data != nullptr);

    util::Uuid uuid;
    const drm::SchemeInitData* scheme_init_data = drm_init_data->Get(uuid);
    DCHECK(scheme_init_data != nullptr);

    const char* pssh_data = scheme_init_data->GetData();
    size_t pssh_len = scheme_init_data->GetLen();

    if (!drm_session_manager_.Join(pssh_data, pssh_len)) {
      // TODO(rmrossi): Notify client we cannot proceed due to
      // lack of a playback license and stop the player.
      LOG(ERROR) << "No playback license for encrypted content!";
      return false;
    }

    // We're good for a license on this track until the next format change.
    track->check_pssh_ = false;
  }

  return true;
}

void DashThread::PopulateFrameInfoCrypto(struct DashFrameInfo* fi,
                                         const SampleHolder& sample_holder) {
  if (!sample_holder.IsEncrypted())
    return;

  const CryptoInfo* crypto_info = sample_holder.GetCryptoInfo();

  DCHECK(crypto_info->GetNumBytesClear().size() ==
         crypto_info->GetNumBytesEncrypted().size());

  fi->iv_len = crypto_info->GetIv().size();
  fi->key_id_len = crypto_info->GetKey().size();
  fi->subsample_count = crypto_info->GetNumBytesClear().size();

  scratch_iv_.resize(fi->iv_len);
  scratch_key_id_.resize(fi->key_id_len);
  scratch_clear_bytes_.resize(fi->subsample_count);
  scratch_enc_bytes_.resize(fi->subsample_count);

  fi->iv = scratch_iv_.data();
  fi->key_id = scratch_key_id_.data();
  fi->clear_bytes = scratch_clear_bytes_.data();
  fi->enc_bytes = scratch_enc_bytes_.data();

  memcpy(scratch_iv_.data(), crypto_info->GetIv().data(), fi->iv_len);
  memcpy(scratch_key_id_.data(), crypto_info->GetKey().data(), fi->key_id_len);
  memcpy(scratch_clear_bytes_.data(), crypto_info->GetNumBytesClear().data(),
         fi->subsample_count * sizeof(int));
  memcpy(scratch_enc_bytes_.data(), crypto_info->GetNumBytesEncrypted().data(),
         fi->subsample_count * sizeof(int));
}

// TODO(rmrossi): This way of getting data through the C API will be
// deprecated.  Instead of the API giving us a buffer to populate with
// data drained from the sample holder, we will give a pointer to where the
// sample data resides and let the player place a hold on it (so it doesn't
// get expunged from the sample queue until the decoder is done with it) and
// release it later.
int DashThread::CopyFrameImpl(void* buffer,
                              int buffer_len,
                              struct DashFrameInfo* fi) {
  memset(fi, 0, sizeof(struct DashFrameInfo));

  if (state_ != STATE_BUFFERING) {
    DLOG(INFO) << "not buffering";
    return -1;
  }

  if (!current_track_) {
    if (!FillTrackSampleHolders()) {
      LOG(INFO) << "All tracks report END_OF_STREAM; playback ended";

      is_eos_ = true;

      qoe_manager_->ReportVideoEnded();
      SetState(STATE_ENDED);
      return 0;
    }

    current_track_ = GetNextSampleTrack();

    if (!current_track_)
      return -1;

    if (!MaybeCheckPssh(current_track_)) {
      // Need to drop the sample if we can't decrypt it
      return 0;
    }

    fi->flags |= DASH_FRAME_INFO_FLAG_FIRST_FRAGMENT;
    PopulateFrameInfoCrypto(fi, current_track_->sample_holder_);
  }

  const SampleHolder* sample_holder = &current_track_->sample_holder_;

  int32_t sample_remaining =
      sample_holder->GetWrittenSize() - sample_holder_consumed_;

  // Drain as much as we can with the space we were given.
  int32_t num_to_write = std::min(buffer_len, sample_remaining);
  const uint8_t* dest = sample_holder->GetBuffer() + sample_holder_consumed_;
  memcpy(buffer, dest, num_to_write);
  sample_holder_consumed_ += num_to_write;

  fi->type = current_track_->frame_type_;

  fi->width = current_track_->format_holder_.format->GetWidth();
  fi->height = current_track_->format_holder_.format->GetHeight();

  int64_t sample_time_us =
      sample_holder->GetTimeUs() - GetSampleOffsetMs() * 1000;
  util::PresentationTime pt = util::PresentationTimeFromUs(sample_time_us);

  fi->pts = pt.pts;
  util::MediaDuration md =
      util::MediaDurationFromUs(sample_holder->GetDurationUs());
  fi->duration = md.md;
  fi->frame_len = sample_holder->GetWrittenSize();

  if (num_to_write == sample_remaining) {
    sample_holder_consumed_ = 0;
    current_track_->has_sample_ = false;
    current_track_->sample_holder_.ClearData();
    current_track_ = nullptr;  // Next time, pick a new sample
    fi->flags |= DASH_FRAME_INFO_FLAG_LAST_FRAGMENT;
  }

  VLOG(4) << "Frame read: " << num_to_write << " bytes"
          << "; time " << base::TimeDelta::FromMicroseconds(pt.pts)
          << " duration " << base::TimeDelta::FromMicroseconds(md.md)
          << " flags " << fi->flags << " type " << fi->type;
  return num_to_write;
}

int DashThread::GetCCCodecSettingsImpl(DashCCCodecSettings* settings) {
  // The only cc type we understand is RAWCC. If this changes, we need to
  // ensure we add TEXT to the codecs to wait for before allowing the
  // player to get here. See HaveCodecs().
  settings->cc_codec = DASH_CC_RAWCC;
  return 0;
}

MediaTimeMs DashThread::GetFirstTimeMsImpl() {
  return GetSampleOffsetMs();
}

MediaDurationMs DashThread::GetDurationMsImpl() {
  return duration_.InMilliseconds();
}

int DashThread::SeekImpl(MediaTimeMs time_ms) {
  bool is_seek_to_start = time_ms == 0;
  time_ms += GetSampleOffsetMs();

  static const base::TimeDelta kMinimumSeekDistance =
      base::TimeDelta::FromSeconds(2);

  base::TimeDelta seek_time = base::TimeDelta::FromMilliseconds(time_ms);

  LOG(INFO) << "Seek to position " << seek_time;

  // We adjust the seek time to match with a video segment boundary so that we
  // are guaranteed a key frame. All audio samples are key frames, so we can
  // freely seek an audio track, thus video is the important one to align. We
  // also assume that text tracks are not a problem.
  //
  // TODO(adewhurst): Support decoder-only frames so that we can seek into any
  //                  position we want. That will also handle possible
  //                  audio/text codecs that are not entirely composed of key
  //                  frames.
  for (auto& track : tracks_) {
    if (track.frame_type_ == DASH_FRAME_TYPE_VIDEO) {
      seek_time = track.chunk_source_->GetAdjustedSeek(seek_time);
      break;
    }
  }

  VLOG(1) << "Adjusted seek position " << seek_time;

  // Always allow seek back to 0 but ignore seeks too close to our current
  // position.
  if ((seek_time - decoder_position_).magnitude() < kMinimumSeekDistance &&
      !is_seek_to_start) {
    LOG(INFO) << "Seek too close to current position. Not seeking.";
    return -1;
  }

  if (playback_rate_.IsTrick()) {
    LOG(INFO) << "Can't seek while tricking.";
    return -1;
  }

  decoder_position_ = seek_time;
  reader_position_ = seek_time;
  seek_position_ = seek_time;
  media_time_last_value_ms_ = seek_time.InMicroseconds();
  decoder_media_time_last_value_ms_ = 0;

  if (state_ != STATE_BUFFERING) {
    initial_time_ = seek_time;
    return 0;
  }

  if (player_callbacks_.decoder_flush_func) {
    player_callbacks_.decoder_flush_func(context_);
  }

  for (auto& track : tracks_) {
    if (track.renderer_->GetState() == TrackRenderer::STARTED) {
      track.renderer_->Stop();
    }
    if (track.renderer_->GetState() == TrackRenderer::ENABLED) {
      track.renderer_->SeekTo(reader_position_);
      track.renderer_->Start();
    }
  }

  // In case we seek when part way through writing out a frame, clear that out
  // (we just flushed the decoder)
  current_track_ = nullptr;
  sample_holder_consumed_ = 0;
  for (TrackContext& track : tracks_) {
    track.sample_holder_.ClearData();
    track.has_sample_ = false;
  }

  media_time_ready_ = false;

  // Run the buffering logic
  task_runner()->PostTask(FROM_HERE, base::Bind(&DashThread::Update,
                                                base::Unretained(this), false));

  if (qoe_manager_) {
    qoe_manager_->SetMediaPos(decoder_position_);
  }

  ReportPlaybackState(DASH_STREAM_STATE_SEEKING);
  return 0;
}

void DashThread::SetPlaybackRateDisableTracks(float target_rate) {
  if (playback_rate_.rate() == target_rate) {
    playback_rate_waiter_.Signal();
    return;
  }

  // Don't allow buffering while we disable tracks.
  SetState(STATE_READY);

  // Stop and Disable all track renderers.
  // TODO(rmrossi): If we're in trick mode already and the target rate would
  // pick the same representation we're currently getting chunks from, there's
  // no reason to disable tracks or flush the decoder. To do this, however, we
  // need to consult the evaluator with the available representations which we
  // don't have easy access to at this level. Plumb this request for info
  // through the track renderer later.
  pending_disable_.clear();
  TrackRenderer::RendererState state;
  for (auto& track : tracks_) {
    state = track.renderer_->GetState();
    if (state == TrackRenderer::STARTED) {
      track.renderer_->Stop();
    }
    state = track.renderer_->GetState();
    if (state == TrackRenderer::ENABLED) {
      pending_disable_.insert(track.renderer_.get());
    }
  }

  if (pending_disable_.empty()) {
    LOG(ERROR) << "expected at least one track to disable";
    playback_rate_waiter_.Signal();
    return;
  }

  for (TrackRenderer* renderer : pending_disable_) {
    base::Closure disable_done = base::Bind(
        &DashThread::SetPlaybackRateEnableTracks, base::Unretained(this),
        target_rate, base::Unretained(renderer));
    renderer->Disable(&disable_done);
  }
}

void DashThread::SetPlaybackRateEnableTracks(float target_rate,
                                             TrackRenderer* disabled_renderer) {
  auto i = pending_disable_.find(disabled_renderer);
  DCHECK(i != pending_disable_.end());
  pending_disable_.erase(disabled_renderer);

  if (pending_disable_.size() > 0) {
    // Still waiting for more renderers to be disabled.
    return;
  }

  DCHECK_NE(playback_rate_.rate(), target_rate);

  player_callbacks_.decoder_flush_func(context_);

  // Back to buffering state.
  SetState(STATE_BUFFERING);

  // Let copy frame figure out whether this is true again.
  is_eos_ = false;

  playback_rate_.set_rate(target_rate);

  for (auto& track : tracks_) {
    if (track.frame_type_ == DASH_FRAME_TYPE_VIDEO ||
        playback_rate_.IsNormal()) {
      track.track_criteria_->prefer_trick = playback_rate_.IsTrick();
      track.renderer_->Enable(track.track_criteria_.get(),
                              decoder_position_.InMicroseconds(), false);
      track.renderer_->Start();
      track.sample_holder_.ClearData();
      track.has_sample_ = false;
    }
  }

  media_time_ready_ = false;
  sample_holder_consumed_ = 0;
  current_track_ = nullptr;

  playback_rate_waiter_.Signal();
  Update(false);
}

int DashThread::GetStreamCountsImpl(int* num_video_streams,
                                    int* num_audio_streams,
                                    int* num_cc_streams) {
  // TODO(rmrossi): TBD
  *num_video_streams = 1;
  *num_audio_streams = 1;
  *num_cc_streams = 0;
  return 1;
}

bool DashThread::MakeLicenseRequest(const std::string& key_message_blob,
                                    std::string* out_response) {
  return license_fetcher_.Fetch(key_message_blob, out_response);
}

void DashThread::ReportPlaybackState(DashStreamState state) {
  if (!qoe_manager_) {
    return;
  }

  LOG(INFO) << "DashThread::ReportPlaybackState: " << state;
  switch (state) {
    case DASH_STREAM_STATE_BUFFERING:
      qoe_manager_->ReportBuffering();
      break;
    case DASH_STREAM_STATE_PLAYING:
      qoe_manager_->ReportVideoPlaying();
      break;
    case DASH_STREAM_STATE_PAUSED:
      qoe_manager_->ReportVideoPaused();
      break;
    case DASH_STREAM_STATE_SEEKING:
      qoe_manager_->ReportVideoSeeking();
      break;
    default:
      LOG(ERROR) << "Unhandled state in ReportPlaybackState: " << state;
      break;
  }
}

void DashThread::ReportPlaybackError(DashPlaybackErrorCode code,
                                     const std::string& error_string,
                                     bool is_fatal) {
  // Translate from our published playback error code to qoe's
  // VideoErrorCode
  qoe::VideoErrorCode qoe_code;
  switch (code) {
    case DASH_VIDEO_MEDIA_PLAYER_AUDIO_INIT_ERROR:
      qoe_code = qoe::VideoErrorCode::FRAMEWORK_MEDIA_PLAYER_AUDIO_INIT_ERROR;
      break;
    case DASH_VIDEO_MEDIA_PLAYER_VIDEO_INIT_ERROR:
      qoe_code = qoe::VideoErrorCode::FRAMEWORK_MEDIA_PLAYER_VIDEO_INIT_ERROR;
      break;
    case DASH_VIDEO_MEDIA_PLAYER_PLAYBACK_ERROR:
      qoe_code = qoe::VideoErrorCode::FRAMEWORK_MEDIA_PLAYER_PLAYBACK_ERROR;
      break;
    case DASH_VIDEO_MEDIA_DRM_ERROR:
      qoe_code = qoe::VideoErrorCode::FRAMEWORK_MEDIA_PLAYER_DRM_ERROR;
      break;
    default:
      qoe_code = qoe::VideoErrorCode::UNKNOWN_ERROR;
      break;
  }
  ReportPlaybackError(qoe_code, error_string, is_fatal);
}

void DashThread::ReportPlaybackError(qoe::VideoErrorCode code,
                                     const std::string& error_string,
                                     bool is_fatal) {
  qoe_manager_->ReportVideoError(code, error_string, is_fatal);
}

void DashThread::FormatGiven(TrackContext* track, const MediaFormat* format) {
  CHECK(track != nullptr);
  track->upstream_format_.reset(new MediaFormat(*format));

  // We have codecs for both audio and video. Release the waiter.
  if (HaveCodecs()) {
    codec_waiter_.Signal();
  }
}

void DashThread::NewBandwidthEstimate(base::TimeDelta elapsed,
                                      int64_t bytes,
                                      int64_t bitrate) {
  base::TimeTicks now = base::TimeTicks::Now();

  if (last_bandwidth_estimate_ + kBandwidthEstimateDelay <= now) {
    LOG(INFO) << "Current bandwidth "
              << base::StringPrintf("%" PRId64 ".%06" PRId64 " Mbps",
                                    bitrate / 1000000, bitrate % 1000000);
    last_bandwidth_estimate_ = now;
  }
}

bool DashThread::HaveCodecs() const {
  bool have_audio = false;
  bool have_video = false;
  for (auto& track : tracks_) {
    if (track.frame_type_ == DASH_FRAME_TYPE_AUDIO &&
        track.upstream_format_.get() != nullptr) {
      have_audio = true;
    } else if (track.frame_type_ == DASH_FRAME_TYPE_VIDEO &&
               track.upstream_format_.get() != nullptr) {
      have_video = true;
    }
  }
  return have_audio && have_video;
}

}  // namespace ndash
