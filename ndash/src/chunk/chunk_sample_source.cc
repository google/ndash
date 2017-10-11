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

#include "chunk/chunk_sample_source.h"

#include <limits>

#include "base/bind.h"
#include "base/time/time.h"
#include "extractor/default_track_output.h"
#include "media_format.h"
#include "playback_rate.h"
#include "upstream/data_spec.h"
#include "upstream/loader_thread.h"
#include "util/util.h"

namespace {
constexpr int64_t kNoResetPending = std::numeric_limits<int64_t>::min();
}

namespace ndash {
namespace chunk {

DefaultLoaderFactory::~DefaultLoaderFactory() {}

std::unique_ptr<upstream::LoaderInterface> DefaultLoaderFactory::CreateLoader(
    ChunkSourceInterface* chunk_source) {
  return std::unique_ptr<upstream::LoaderInterface>(
      new upstream::LoaderThread("Loader:" + chunk_source->GetContentType()));
}

ChunkSampleSource::ChunkSampleSource(
    ChunkSourceInterface* chunk_source,
    LoadControl* load_control,
    const PlaybackRate* playback_rate,
    int32_t buffer_size_contribution,
    ChunkSampleSourceEventListenerInterface* event_listener,
    int32_t event_source_id,
    int32_t min_loadable_retry_count,
    std::unique_ptr<LoaderFactoryInterface> loader_factory)
    : event_source_id_(event_source_id),
      playback_rate_(playback_rate),
      load_control_(load_control),
      chunk_source_(chunk_source),
      buffer_size_contribution_(buffer_size_contribution),
      event_listener_(event_listener),
      min_loadable_retry_count_(min_loadable_retry_count),
      state_(STATE_IDLE),
      downstream_position_us_(0),
      last_seek_position_us_(0),
      pending_reset_position_us_(kNoResetPending),
      last_performed_buffer_operation_(),
      pending_discontinuity_(false),
      loading_finished_(false),
      current_loadable_error_reason_(ChunkLoadErrorReason::NO_ERROR),
      enabled_track_count_(0),
      current_loadable_error_count_(0),
      current_loadable_error_timestamp_(),
      current_load_start_time_(),
      downstream_media_format_(nullptr),
      downstream_format_(nullptr),
      loader_factory_(std::move(loader_factory)),
      loader_(nullptr) {
  sample_queue_.reset(
      new extractor::DefaultTrackOutput(load_control->GetAllocator()));
}

ChunkSampleSource::~ChunkSampleSource() {}

SampleSourceReaderInterface* ChunkSampleSource::Register() {
  DCHECK(state_ == STATE_IDLE);
  state_ = STATE_INITIALIZED;
  return this;
}

bool ChunkSampleSource::Prepare(int64_t position_us) {
  DCHECK(state_ == STATE_INITIALIZED || state_ == STATE_PREPARED);
  if (state_ == STATE_PREPARED) {
    return true;
  } else if (!chunk_source_->Prepare()) {
    return false;
  }
  loader_ = loader_factory_->CreateLoader(chunk_source_);
  if (loader_.get() == nullptr) {
    LOG(FATAL) << "Loader factory returned null!";
    return false;
  }
  state_ = STATE_PREPARED;
  return true;
}

int64_t ChunkSampleSource::GetDurationUs() {
  DCHECK(state_ == STATE_PREPARED || state_ == STATE_ENABLED);
  return chunk_source_->GetDurationUs();
}

void ChunkSampleSource::Enable(const TrackCriteria* track_criteria,
                               int64_t position_us) {
  DCHECK(state_ == STATE_PREPARED);
  enabled_track_count_++;
  DCHECK(enabled_track_count_ == 1);
  state_ = STATE_ENABLED;
  chunk_source_->Enable(track_criteria);
  load_control_->Register(loader_.get(), buffer_size_contribution_);
  downstream_position_us_ = position_us;
  downstream_format_.reset(nullptr);
  downstream_media_format_.reset();
  last_seek_position_us_ = position_us;
  pending_discontinuity_ = false;
  RestartFrom(position_us);
}

void ChunkSampleSource::Disable(const base::Closure* disable_done_callback) {
  DCHECK(state_ == STATE_ENABLED);
  state_ = STATE_DISABLING;

  if (disable_done_callback) {
    disable_done_callback_ = *disable_done_callback;
  }

  if (loader_->IsLoading()) {
    loader_->CancelLoading();
  } else {
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&ChunkSampleSource::DisableAndClear,
                              base::Unretained(this)));
  }
}

void ChunkSampleSource::DisableAndClear() {
  DCHECK(state_ == STATE_DISABLING);
  --enabled_track_count_;
  DCHECK(enabled_track_count_ == 0);

  state_ = STATE_PREPARED;
  load_control_->Unregister(loader_.get());
  chunk_source_->Disable(&media_chunks_);
  sample_queue_->Clear();
  media_chunks_.clear();
  ClearCurrentLoadable();
  load_control_->TrimAllocator();

  if (!disable_done_callback_.is_null()) {
    disable_done_callback_.Run();
    disable_done_callback_.Reset();
  }
}

bool ChunkSampleSource::ContinueBuffering(int64_t position_us) {
  DCHECK(state_ == STATE_ENABLED);
  downstream_position_us_ = position_us;
  chunk_source_->ContinueBuffering(
      base::TimeDelta::FromMicroseconds(position_us));
  UpdateLoadControl();
  return loading_finished_ || !sample_queue_->IsEmpty();
}

int64_t ChunkSampleSource::ReadDiscontinuity() {
  if (pending_discontinuity_) {
    pending_discontinuity_ = false;
    return last_seek_position_us_;
  }
  return NO_DISCONTINUITY;
}

SampleSourceReaderInterface::ReadResult ChunkSampleSource::ReadData(
    int64_t position_us,
    MediaFormatHolder* format_holder,
    SampleHolder* sample_holder) {
  DCHECK(state_ == STATE_ENABLED);
  downstream_position_us_ = position_us;

  if (pending_discontinuity_ || IsPendingReset()) {
    return NOTHING_READ;
  }

  bool have_samples = !sample_queue_->IsEmpty();
  BaseMediaChunk* current_chunk =
      static_cast<BaseMediaChunk*>(media_chunks_.front().get());

  while (have_samples && media_chunks_.size() > 1 &&
         static_cast<BaseMediaChunk*>(media_chunks_.at(1).get())
                 ->first_sample_index() <= sample_queue_->GetReadIndex()) {
    media_chunks_.pop_front();
    current_chunk = static_cast<BaseMediaChunk*>(media_chunks_.front().get());
  }

  // TODO(rmrossi): downstream_format_ can't be a ptr to what comes
  // out of the current chunk because that memory will likely be released while
  // we hold onto a ptr to it.  downstream_format_ is now a copy.  The
  // original code attempted to bypass expensive inequality checks on each
  // iteration by setting the downstream_format_ (java) reference to what came
  // from the chunk regardless of whether it changed or not.  This won't be true
  // for our impl. We will make a comparison each time we arrive here.
  const util::Format* format = current_chunk->format();
  if (format != nullptr && (downstream_format_.get() == nullptr ||
                            !(*format == *downstream_format_))) {
    NotifyDownstreamFormatChanged(format, current_chunk->trigger(),
                                  current_chunk->start_time_us());
    downstream_format_.reset(new util::Format(*format));
  } else if (format == nullptr) {
    downstream_format_.reset(nullptr);
  }

  if (have_samples || current_chunk->is_media_format_final()) {
    // TODO(rmrossi): downstream_media_format_ can't be a ptr to what comes
    // out of the current chunk because that memory will likely be released
    // while we hold onto a ptr to it.  downstream_media_format_ is now a copy.
    // The original code attempted to bypass expensive inequality checks on
    // each iteration by setting the downstream_format_ (java) reference to
    // what came from the chunk regardless of whether it changed or not.  This
    // won't be true for our impl. We will make a comparison each time we
    // arrive here.
    const MediaFormat* media_format = current_chunk->GetMediaFormat();
    if (media_format && media_format != downstream_media_format_.get()) {
      format_holder->format.reset(new MediaFormat(*media_format));
      format_holder->drm_init_data = current_chunk->GetDrmInitData();
      downstream_media_format_ = media_format->AsWeakPtr();
      return FORMAT_READ;
    } else if (!media_format) {
      downstream_media_format_.reset();
    }
  }

  if (!have_samples) {
    if (loading_finished_) {
      return END_OF_STREAM;
    }
    return NOTHING_READ;
  }

  if (sample_queue_->GetSample(sample_holder)) {
    bool decode_only =
        playback_rate_->IsForward()
            ? sample_holder->GetTimeUs() < last_seek_position_us_
            : sample_holder->GetTimeUs() > last_seek_position_us_;
    sample_holder->SetFlags(sample_holder->GetFlags() |
                            (decode_only ? util::kSampleFlagDecodeOnly : 0));
    OnSampleRead(current_chunk, sample_holder);
    return SAMPLE_READ;
  }

  return NOTHING_READ;
}

void ChunkSampleSource::SeekToUs(int64_t position_us) {
  DCHECK(state_ == STATE_ENABLED);

  int64_t current_position_us =
      IsPendingReset() ? pending_reset_position_us_ : downstream_position_us_;
  downstream_position_us_ = position_us;
  last_seek_position_us_ = position_us;
  if (current_position_us == position_us) {
    return;
  }

  // If we're not pending a reset, see if we can seek within the sample queue.
  bool seek_inside_buffer =
      !IsPendingReset() && sample_queue_->SkipToKeyframeBefore(position_us);
  if (seek_inside_buffer) {
    // We succeeded. All we need to do is discard any chunks that we've moved
    // past.
    bool have_samples = !sample_queue_->IsEmpty();
    while (have_samples && media_chunks_.size() > 1 &&
           static_cast<BaseMediaChunk*>(media_chunks_.at(1).get())
                   ->first_sample_index() <= sample_queue_->GetReadIndex()) {
      media_chunks_.pop_front();
    }
  } else {
    // We failed, and need to restart.
    RestartFrom(position_us);
  }
  // Either way, we need to send a discontinuity to the downstream components.
  pending_discontinuity_ = true;
}

bool ChunkSampleSource::CanContinueBuffering() {
  if (current_loadable_error_reason_ != ChunkLoadErrorReason::NO_ERROR &&
      current_loadable_error_count_ > min_loadable_retry_count_) {
    return false;
  } else if (current_loadable_holder_.GetChunk() == nullptr) {
    return chunk_source_->CanContinueBuffering();
  }
  return true;
}

int64_t ChunkSampleSource::GetBufferedPositionUs() {
  DCHECK(state_ == STATE_ENABLED);
  if (IsPendingReset()) {
    return pending_reset_position_us_;
  } else if (loading_finished_) {
    return util::kEndOfTrackUs;
  }
  int64_t largest_parsed_timestamp_us =
      sample_queue_->GetLargestParsedTimestampUs();
  return largest_parsed_timestamp_us == kNoParsedTimestamp
             ? downstream_position_us_
             : largest_parsed_timestamp_us;
}

void ChunkSampleSource::Release() {
  DCHECK(state_ != STATE_ENABLED);
  if (loader_.get() != nullptr && loader_->IsLoading()) {
    loader_->CancelLoading();
  }
  // NOTE: The loader must remain alive as long as any thread that called
  // StartLoading() is still alive. This is necessary to allow the loader's
  // DoneLoad method to execute on that thread with a valid Loader instance.
  state_ = STATE_IDLE;
}

// Called on the thread that called StartLoading
void ChunkSampleSource::LoadComplete(upstream::LoadableInterface* loadable,
                                     upstream::LoaderOutcome outcome) {
  switch (outcome) {
    case upstream::LOAD_COMPLETE:
      OnLoadCompleted(loadable);
      break;
    case upstream::LOAD_ERROR:
      // TODO(rmrossi): Should query loader to get more specific errors.
      OnLoadError(loadable, ChunkLoadErrorReason::GENERIC_ERROR);
      break;
    case upstream::LOAD_CANCELED:
      OnLoadCanceled(loadable);
      break;
    default:
      LOG(FATAL) << "Unknown loader outcome in LoadComplete!";
      break;
  }
}

// Called on the thread that called StartLoading
void ChunkSampleSource::OnLoadCompleted(upstream::LoadableInterface* loadable) {
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta load_duration = now - current_load_start_time_;
  Chunk* current_loadable = current_loadable_holder_.GetChunk();
  chunk_source_->OnChunkLoadCompleted(current_loadable);
  if (IsMediaChunk(current_loadable)) {
    BaseMediaChunk* media_chunk =
        static_cast<BaseMediaChunk*>(current_loadable);
    NotifyLoadCompleted(current_loadable->GetNumBytesLoaded(),
                        media_chunk->type(), media_chunk->trigger(),
                        media_chunk->format(), media_chunk->start_time_us(),
                        media_chunk->end_time_us(), now, load_duration);
  } else {
    NotifyLoadCompleted(current_loadable->GetNumBytesLoaded(),
                        current_loadable->type(), current_loadable->trigger(),
                        current_loadable->format(), -1, -1, now, load_duration);
  }
  ClearCurrentLoadable();
  UpdateLoadControl();
}

// Called on the thread that called StartLoading
void ChunkSampleSource::OnLoadCanceled(upstream::LoadableInterface* loadable) {
  Chunk* current_loadable = current_loadable_holder_.GetChunk();
  NotifyLoadCanceled(current_loadable->GetNumBytesLoaded());
  ClearCurrentLoadable();
  if (state_ == STATE_ENABLED) {
    RestartFrom(pending_reset_position_us_);
  } else {
    DisableAndClear();
  }
}

// Called on the thread that called StartLoading
void ChunkSampleSource::OnLoadError(upstream::LoadableInterface* loadable,
                                    ChunkLoadErrorReason e) {
  current_loadable_error_reason_ = e;
  current_loadable_error_count_++;
  current_loadable_error_timestamp_ = base::TimeTicks::Now();
  NotifyLoadError(e);
  chunk_source_->OnChunkLoadError(current_loadable_holder_.GetChunk(), e);
  UpdateLoadControl();
}

void ChunkSampleSource::OnSampleRead(MediaChunk* mediaChunk,
                                     SampleHolder* sample_holder) {
  // Do nothing.
}

void ChunkSampleSource::RestartFrom(int64_t position_us) {
  pending_reset_position_us_ = position_us;
  loading_finished_ = false;
  if (loader_->IsLoading()) {
    loader_->CancelLoading();
  } else {
    sample_queue_->Clear();
    media_chunks_.clear();
    ClearCurrentLoadable();
    UpdateLoadControl();
  }
}

void ChunkSampleSource::ClearCurrentLoadable() {
  current_loadable_holder_.SetChunk(nullptr);
  ClearCurrentLoadableException();
}

void ChunkSampleSource::ClearCurrentLoadableException() {
  current_loadable_error_reason_ = ChunkLoadErrorReason::NO_ERROR;
  current_loadable_error_count_ = 0;
}

void ChunkSampleSource::UpdateLoadControl() {
  base::TimeTicks now = base::TimeTicks::Now();
  int64_t next_load_position_us = GetNextLoadPositionUs();
  bool is_backed_off =
      current_loadable_error_reason_ != ChunkLoadErrorReason::NO_ERROR;
  bool loading_or_backed_off = loader_->IsLoading() || is_backed_off;

  // If we're not loading or backed off, evaluate the operation if (a) we don't
  // have the next chunk yet and we're not finished, or (b) if the last
  // evaluation was over 2000ms ago.
  if (!loading_or_backed_off &&
      ((current_loadable_holder_.GetChunk() == nullptr &&
        next_load_position_us != -1) ||
       (now - last_performed_buffer_operation_ >
        base::TimeDelta::FromSeconds(2)))) {
    // Perform the evaluation.
    last_performed_buffer_operation_ = now;
    DoChunkOperation();
    bool chunks_discarded =
        DiscardUpstreamMediaChunks(current_loadable_holder_.GetQueueSize());
    // Update the next load position as appropriate.
    if (current_loadable_holder_.GetChunk() == nullptr) {
      // Set loadPosition to -1 to indicate that we don't have anything to load.
      next_load_position_us = -1;
    } else if (chunks_discarded) {
      // Chunks were discarded, so we need to re-evaluate the load position.
      next_load_position_us = GetNextLoadPositionUs();
    }
  }

  // Update the control with our current state, and determine whether we're the
  // next loader.
  bool next_loader =
      load_control_->Update(loader_.get(), downstream_position_us_,
                            next_load_position_us, loading_or_backed_off);
  if (is_backed_off) {
    base::TimeDelta elapsed = now - current_loadable_error_timestamp_;
    if (elapsed >= GetRetryDelayMillis(current_loadable_error_count_)) {
      ResumeFromBackOff();
    }
    return;
  }
  if (!loader_->IsLoading() && next_loader) {
    MaybeStartLoading();
  }
}

int64_t ChunkSampleSource::GetNextLoadPositionUs() {
  if (IsPendingReset()) {
    return pending_reset_position_us_;
  } else {
    return loading_finished_ ? -1 : media_chunks_.back()->end_time_us();
  }
}

void ChunkSampleSource::ResumeFromBackOff() {
  current_loadable_error_reason_ = ChunkLoadErrorReason::NO_ERROR;
  Chunk* backed_off_chunk = current_loadable_holder_.GetChunk();
  if (!IsMediaChunk(backed_off_chunk)) {
    DoChunkOperation();
    DiscardUpstreamMediaChunks(current_loadable_holder_.GetQueueSize());
    if (current_loadable_holder_.GetChunk() == backed_off_chunk) {
      // Chunk was unchanged. Resume loading.
      loader_->StartLoading(
          backed_off_chunk,
          base::Bind(&ChunkSampleSource::LoadComplete, base::Unretained(this)));
    } else {
      // Chunk was changed. Notify that the existing load was canceled.
      NotifyLoadCanceled(backed_off_chunk->GetNumBytesLoaded());
      // Start loading the replacement.
      MaybeStartLoading();
    }
    return;
  }

  if (backed_off_chunk == media_chunks_.front().get()) {
    // We're not able to clear the first media chunk, so we have no choice but
    // to continue loading it.
    loader_->StartLoading(
        backed_off_chunk,
        base::Bind(&ChunkSampleSource::LoadComplete, base::Unretained(this)));
    return;
  }

  // The current loadable is the last media chunk. Remove it before we invoke
  // the chunk source, and add it back again afterwards.
  std::unique_ptr<MediaChunk> removed_chunk = std::move(media_chunks_.back());
  media_chunks_.pop_back();

  DCHECK(backed_off_chunk == removed_chunk.get());
  DoChunkOperation();
  media_chunks_.push_back(std::move(removed_chunk));

  if (current_loadable_holder_.GetChunk() == backed_off_chunk) {
    // Chunk was unchanged. Resume loading.
    loader_->StartLoading(
        backed_off_chunk,
        base::Bind(&ChunkSampleSource::LoadComplete, base::Unretained(this)));
  } else {
    // Chunk was changed. Notify that the existing load was canceled.
    NotifyLoadCanceled(backed_off_chunk->GetNumBytesLoaded());
    // This call will remove and release at least one chunk from the end of
    // mediaChunks. Since the current loadable is the last media chunk, it is
    // guaranteed to be removed.
    DiscardUpstreamMediaChunks(current_loadable_holder_.GetQueueSize());
    ClearCurrentLoadableException();
    MaybeStartLoading();
  }
}

void ChunkSampleSource::MaybeStartLoading() {
  Chunk* current_loadable = current_loadable_holder_.GetChunk();
  if (current_loadable == nullptr) {
    // Nothing to load.
    return;
  }
  current_load_start_time_ = base::TimeTicks::Now();
  if (IsMediaChunk(current_loadable)) {
    std::unique_ptr<Chunk> chunk_up = current_loadable_holder_.TakeChunk();
    // We should never be trying to push the same media chunk onto our queue
    // so taking ownership should always succeed.
    DCHECK(chunk_up.get() != nullptr);
    current_loadable = chunk_up.release();
    BaseMediaChunk* media_chunk =
        static_cast<BaseMediaChunk*>(current_loadable);
    media_chunk->Init(sample_queue_.get());
    media_chunks_.push_back(std::unique_ptr<MediaChunk>(media_chunk));
    if (IsPendingReset()) {
      pending_reset_position_us_ = kNoResetPending;
    }
    NotifyLoadStarted(media_chunk->data_spec()->length, media_chunk->type(),
                      media_chunk->trigger(), media_chunk->format(),
                      media_chunk->start_time_us(), media_chunk->end_time_us());
  } else {
    NotifyLoadStarted(current_loadable->data_spec()->length,
                      current_loadable->type(), current_loadable->trigger(),
                      current_loadable->format(), -1, -1);
  }
  loader_->StartLoading(
      current_loadable,
      base::Bind(&ChunkSampleSource::LoadComplete, base::Unretained(this)));
}

void ChunkSampleSource::DoChunkOperation() {
  current_loadable_holder_.SetEndOfStream(false);
  current_loadable_holder_.SetQueueSize(media_chunks_.size());
  chunk_source_->GetChunkOperation(
      &media_chunks_,
      pending_reset_position_us_ != kNoResetPending
          ? base::TimeDelta::FromMicroseconds(pending_reset_position_us_)
          : base::TimeDelta::FromMicroseconds(downstream_position_us_),
      &current_loadable_holder_);
  loading_finished_ = current_loadable_holder_.IsEndOfStream();
}

bool ChunkSampleSource::DiscardUpstreamMediaChunks(int32_t queue_length) {
  if (media_chunks_.size() <= queue_length) {
    return false;
  }
  int64_t start_time_us = 0;
  int64_t end_time_us = media_chunks_.back()->end_time_us();

  std::unique_ptr<MediaChunk> removed;
  loading_finished_ = false;
  while (media_chunks_.size() > queue_length) {
    removed = std::move(media_chunks_.back());
    media_chunks_.pop_back();
  }
  start_time_us = removed->start_time_us();

  BaseMediaChunk* removed_bmc = static_cast<BaseMediaChunk*>(removed.get());
  sample_queue_->DiscardUpstreamSamples(removed_bmc->first_sample_index());

  NotifyUpstreamDiscarded(start_time_us, end_time_us);
  return true;
}

bool ChunkSampleSource::IsPendingReset() {
  return pending_reset_position_us_ != kNoResetPending;
}

void ChunkSampleSource::NotifyLoadStarted(int64_t length,
                                          int32_t type,
                                          int32_t trigger,
                                          const util::Format* format,
                                          int64_t media_start_time_us,
                                          int64_t media_end_time_us) {
  if (event_listener_ != nullptr) {
    event_listener_->OnLoadStarted(event_source_id_, length, type, trigger,
                                   format, UsToMs(media_start_time_us),
                                   UsToMs(media_end_time_us));
  }
}

void ChunkSampleSource::NotifyLoadCompleted(int64_t bytes_loaded,
                                            int32_t type,
                                            int32_t trigger,
                                            const util::Format* format,
                                            int64_t media_start_time_us,
                                            int64_t media_end_time_us,
                                            base::TimeTicks elapsed_real_time,
                                            base::TimeDelta load_duration) {
  if (event_listener_ != nullptr) {
    event_listener_->OnLoadCompleted(
        event_source_id_, bytes_loaded, type, trigger, format,
        UsToMs(media_start_time_us), UsToMs(media_end_time_us),
        elapsed_real_time, load_duration);
  }
}

void ChunkSampleSource::NotifyLoadCanceled(int64_t bytes_loaded) {
  if (event_listener_ != nullptr) {
    event_listener_->OnLoadCanceled(event_source_id_, bytes_loaded);
  }
}

void ChunkSampleSource::NotifyLoadError(ChunkLoadErrorReason e) {
  if (event_listener_ != nullptr) {
    event_listener_->OnLoadError(event_source_id_, e);
  }
}

void ChunkSampleSource::NotifyUpstreamDiscarded(int64_t media_start_time_us,
                                                int64_t media_end_time_us) {
  if (event_listener_ != nullptr) {
    event_listener_->OnUpstreamDiscarded(event_source_id_,
                                         UsToMs(media_start_time_us),
                                         UsToMs(media_end_time_us));
  }
}

void ChunkSampleSource::NotifyDownstreamFormatChanged(
    const util::Format* format,
    int32_t trigger,
    int64_t position_us) {
  if (event_listener_ != nullptr) {
    event_listener_->OnDownstreamFormatChanged(event_source_id_, format,
                                               trigger, UsToMs(position_us));
  }
}

}  // namespace chunk
}  // namespace ndash
