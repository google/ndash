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

#ifndef NDASH_CHUNK_SAMPLE_SOURCE_H_
#define NDASH_CHUNK_SAMPLE_SOURCE_H_

#include <algorithm>
#include <cstdint>
#include <deque>
#include <limits>
#include <memory>

#include "base/memory/weak_ptr.h"
#include "chunk/base_media_chunk.h"
#include "chunk/chunk_operation_holder.h"
#include "chunk/chunk_sample_source_event_listener.h"
#include "chunk/chunk_source.h"
#include "extractor/default_track_output.h"
#include "load_control.h"
#include "media_format_holder.h"
#include "sample_holder.h"
#include "sample_source.h"
#include "upstream/loader.h"

namespace ndash {

class PlaybackRate;
struct TrackCriteria;

namespace chunk {

constexpr int64_t kNoResetPending = std::numeric_limits<int64_t>::min();
// The default minimum number of times to retry loading data prior to failing.
constexpr int32_t kDefaultMinLoadableRetryCount = 3;

constexpr int64_t kNoParsedTimestamp = std::numeric_limits<int64_t>::min();

class LoaderFactoryInterface {
 public:
  virtual ~LoaderFactoryInterface() {}
  virtual std::unique_ptr<upstream::LoaderInterface> CreateLoader(
      ChunkSourceInterface* chunk_source) = 0;
};

class DefaultLoaderFactory : public LoaderFactoryInterface {
 public:
  ~DefaultLoaderFactory() override;
  std::unique_ptr<upstream::LoaderInterface> CreateLoader(
      ChunkSourceInterface* chunk_source) override;
};

// A SampleSource that loads media in Chunks, which are themselves obtained
// from a ChunkSource.
class ChunkSampleSource : public SampleSourceInterface,
                          public SampleSourceReaderInterface {
 public:
  //  chunk_source A ChunkSource from which chunks to load are obtained.
  //  load_control Controls when the source is permitted to load data.
  //  buffer_size_contribution The contribution of this source to the media
  //  buffer, in bytes.
  //  event_listener A listener of events. May be null if delivery of events is
  //  not required.
  //  event_source_id An identifier that gets passed to
  //    ChunkSampleSourceEventListenerInterface methods.
  //  min_loadable_retry_count The minimum number of times that the source
  //  should retry a load before propagating an error.
  ChunkSampleSource(
      ChunkSourceInterface* chunk_source,
      LoadControl* load_control,
      const PlaybackRate* playback_rate,
      int32_t buffer_size_contribution,
      ChunkSampleSourceEventListenerInterface* event_listener = nullptr,
      int32_t event_source_id = 0,
      int32_t min_loadable_retry_count = kDefaultMinLoadableRetryCount,
      std::unique_ptr<LoaderFactoryInterface> loader_factory =
          std::unique_ptr<LoaderFactoryInterface>(new DefaultLoaderFactory));
  ~ChunkSampleSource() override;

  SampleSourceReaderInterface* Register() override;
  bool Prepare(int64_t position_us) override;
  int64_t GetDurationUs() override;
  void Enable(const TrackCriteria* track_criteria,
              int64_t position_us) override;
  void Disable(const base::Closure* disable_done_callback = nullptr) override;
  bool ContinueBuffering(int64_t position_us) override;
  int64_t ReadDiscontinuity() override;
  SampleSourceReaderInterface::ReadResult ReadData(
      int64_t position_us,
      MediaFormatHolder* format_holder,
      SampleHolder* sample_holder) override;
  void SeekToUs(int64_t position_us) override;
  bool CanContinueBuffering() override;

  int64_t GetBufferedPositionUs() override;
  void Release() override;
  void LoadComplete(upstream::LoadableInterface* loadable,
                    upstream::LoaderOutcome outcome);
  void OnLoadCompleted(upstream::LoadableInterface* loadable);
  void OnLoadCanceled(upstream::LoadableInterface* loadable);
  void OnLoadError(upstream::LoadableInterface* loadable,
                   ChunkLoadErrorReason e);

 private:
  // Called when a sample has been read. Can be used to perform any
  // modifications necessary before the sample is returned.
  //
  // mediaChunk The chunk from which the sample was obtained.
  // sample_holder Holds the read sample.
  void OnSampleRead(MediaChunk* mediaChunk, SampleHolder* sample_holder);
  void RestartFrom(int64_t position_us);
  void ClearCurrentLoadable();
  void ClearCurrentLoadableException();
  void UpdateLoadControl();

  // Gets the next load time, assuming that the next load starts where the
  // previous chunk ended (or from the pending reset time, if there is one).
  int64_t GetNextLoadPositionUs();

  // Resumes loading.
  // If the ChunkSource returns a chunk equivalent to the backed off chunk B,
  // then the loading of B will be resumed. In all other cases B will be
  // discarded and the new chunk will be loaded.
  void ResumeFromBackOff();

  void MaybeStartLoading();

  // Sets up the current_loadable_holder_, passes it to the chunk source to
  // cause it to be updated with the next operation, and updates
  // loading_finished _if the end of the stream is reached.
  void DoChunkOperation();

  // Discard upstream media chunks until the queue length is equal to the
  // length specified.
  // queue_length The desired length of the queue.
  // Returns true if chunks were discarded. False otherwise.
  bool DiscardUpstreamMediaChunks(int32_t queue_length);

  bool IsMediaChunk(Chunk* chunk) { return chunk->type() == Chunk::kTypeMedia; }

  bool IsPendingReset();

  base::TimeDelta GetRetryDelayMillis(int32_t error_count) {
    // TODO(rmrossi): These should be configurable.
    return std::min(base::TimeDelta::FromSeconds(1) * (error_count - 1),
                    base::TimeDelta::FromSeconds(5));
  }

  int64_t UsToMs(int64_t time_us) { return time_us / util::kMicrosPerMs; }

  void NotifyLoadStarted(int64_t length,
                         int32_t type,
                         int32_t trigger,
                         const util::Format* format,
                         int64_t media_start_time_us,
                         int64_t media_end_time_us);
  void NotifyLoadCompleted(int64_t bytes_loaded,
                           int32_t type,
                           int32_t trigger,
                           const util::Format* format,
                           int64_t media_start_time_us,
                           int64_t media_end_time_us,
                           base::TimeTicks elapsed_real_time_ms,
                           base::TimeDelta load_duration_ms);
  void NotifyLoadCanceled(int64_t bytes_loaded);
  void NotifyLoadError(ChunkLoadErrorReason e);
  void NotifyUpstreamDiscarded(int64_t media_start_time_us,
                               int64_t media_end_time_us);
  void NotifyDownstreamFormatChanged(const util::Format* format,
                                     int32_t trigger,
                                     int64_t position_us);

  // Helper method to finish disabling the chunk source and clearing
  // out the sample queue and media chunks queue.
  void DisableAndClear();

  enum ChunkState {
    STATE_IDLE = 0,
    STATE_INITIALIZED = 1,
    STATE_PREPARED = 2,
    STATE_ENABLED = 3,
    STATE_DISABLING = 4,
  };

  std::unique_ptr<extractor::DefaultTrackOutput> sample_queue_;
  int32_t event_source_id_;
  const PlaybackRate* const playback_rate_;
  LoadControl* load_control_;
  ChunkSourceInterface* chunk_source_;
  ChunkOperationHolder current_loadable_holder_;
  std::deque<std::unique_ptr<MediaChunk>> media_chunks_;

  int32_t buffer_size_contribution_;
  ChunkSampleSourceEventListenerInterface* event_listener_;
  int32_t min_loadable_retry_count_;

  ChunkState state_;
  int64_t downstream_position_us_;
  int64_t last_seek_position_us_;
  int64_t pending_reset_position_us_;
  base::TimeTicks last_performed_buffer_operation_;
  bool pending_discontinuity_;

  bool loading_finished_;
  ChunkLoadErrorReason current_loadable_error_reason_;
  int32_t enabled_track_count_;
  int32_t current_loadable_error_count_;
  base::TimeTicks current_loadable_error_timestamp_;
  base::TimeTicks current_load_start_time_;

  base::WeakPtr<const MediaFormat> downstream_media_format_;
  std::unique_ptr<const util::Format> downstream_format_;

  std::unique_ptr<LoaderFactoryInterface> loader_factory_;
  std::unique_ptr<upstream::LoaderInterface> loader_;

  base::Closure disable_done_callback_;
};

}  // namespace chunk
}  // namespace ndash

#endif  // NDASH_CHUNK_SAMPLE_SOURCE_H_
