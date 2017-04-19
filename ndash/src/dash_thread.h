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

#ifndef NDASH_DASH_THREAD_H_
#define NDASH_DASH_THREAD_H_

#include <atomic>
#include <cstdint>

#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "chunk/chunk_sample_source.h"
#include "chunk/format_evaluator.h"
#include "dash/dash_chunk_source.h"
#include "dash/dash_track_selector.h"
#include "drm/drm_session_manager.h"
#include "drm/license_fetcher.h"
#include "load_control.h"
#include "manifest_fetcher.h"
#include "ndash.h"
#include "playback_rate.h"
#include "player_attributes.h"
#include "qoe/qoe_manager.h"
#include "sample_holder.h"
#include "sample_source.h"
#include "track_criteria.h"
#include "track_renderer.h"
#include "upstream/allocator.h"
#include "upstream/data_source.h"
#include "upstream/default_bandwidth_meter.h"

namespace ndash {

class DashThread : public base::Thread,
                   public EventListenerInterface,
                   public chunk::ChunkSampleSourceEventListenerInterface {
 public:
  enum PlayerState {
    STATE_IDLE = 1,
    STATE_PREPARING = 2,
    STATE_BUFFERING = 3,
    STATE_READY = 4,
    STATE_ENDED = 5,
  };

  explicit DashThread(const std::string& name, void* context);
  ~DashThread() override;

  // Load the manifest pointed to by |url|, process it and begin buffering.
  // This method should be called by the consumer (player) thread.
  // Returns false on fatal error.
  bool Load(const char* url, int32_t initialTimeSec);

  // Unloads the player. Renderer states will transition back to the
  // TrackRenderer::RELEASED state.  Loaders are canceled and stopped. This
  // method will block until tear down is complete. This method should be
  // called by the consumer (API) thread.
  void Unload();

  // Set callback functions decoder control functions. This method should be
  // called by the consumer (C API) thread prior to calling Load().
  void SetPlayerCallbacks(const DashPlayerCallbacks* callbacks);

  // Update the callback context argument from what it was set during initial
  // construction.
  void SetPlayerCallbackContext(void* context);

  // Returns false on error.
  bool SetAttribute(const std::string& attr_name,
                    const std::string& attr_value);

  // Set the playback rate. This method should be called by the consumer
  // (API) thread.
  void SetPlaybackRate(float target_rate);

  // Manifest EventListenerInterface Callbacks. These methods are called by
  // the DashThread's task runner.
  void OnManifestRefreshStarted() override;
  void OnManifestRefreshed() override;
  void OnManifestError(ManifestFetcher::ManifestFetchError error) override;

  // Static member functions for our  API C code to get us back into c++ private
  // member functions. These methods are called by the API thread (unless
  // specified otherwise)
  //
  // All implementations currently call indirectly via posting a task to the
  // dash thread's task runner (unless specified otherwise) and wait for the
  // task to finish before returning.
  static int GetVideoCodecSettings(DashThread* dash,
                                   DashVideoCodecSettings* codec_settings);
  static int GetAudioCodecSettings(DashThread* dash,
                                   DashAudioCodecSettings* codec_settings);
  bool IsEOS() const;

  static int CopyFrame(DashThread* dash,
                       void* buffer,
                       int bufferlen,
                       struct DashFrameInfo* fi);
  static int GetCCCodecSettings(DashThread* dash,
                                DashCCCodecSettings* settings);
  static MediaTimeMs GetFirstTime(DashThread* dash);
  static MediaDurationMs GetDurationMs(DashThread* dash);
  static int Seek(DashThread* dash, MediaTimeMs time);
  static int GetStreamCounts(DashThread* dash,
                             int* num_videostreams,
                             int* num_audiostreams,
                             int* num_cc_streams);
  static int SetVideoStream(DashThread* dash, int stream);
  static int SetAudioStream(DashThread* dash, int stream);
  static int SetSpuStream(DashThread* dash, int stream);

  // Make a request for a playback license from the license server.
  // key_message_blob - The body of the request constructed by the CDM.
  // out_response - A destination string for the playback license.
  // This method will be called on the DrmSessionManager's worker thread.
  bool MakeLicenseRequest(const std::string& key_message_blob,
                          std::string* out_response);

  void ReportPlaybackState(DashStreamState state);

  // Public report function that takes our published subset of playback error
  // codes.
  void ReportPlaybackError(DashPlaybackErrorCode code,
                           const std::string& error_string,
                           bool is_fatal);

  // ChunkSampleSourceEventListenerInterface methods
  void OnLoadStarted(int32_t source_id,
                     int64_t length,
                     int32_t type,
                     int32_t trigger,
                     const util::Format* format,
                     int64_t media_start_time_ms,
                     int64_t media_end_time_ms) override;
  void OnLoadCompleted(int32_t source_id,
                       int64_t bytes_loaded,
                       int32_t type,
                       int32_t trigger,
                       const util::Format* format,
                       int64_t media_start_time_ms,
                       int64_t media_end_time_ms,
                       base::TimeTicks elapsed_real_time,
                       base::TimeDelta load_duration);
  void OnLoadCanceled(int32_t source_id, int64_t bytesLoaded) override;
  void OnLoadError(int32_t source_id, chunk::ChunkLoadErrorReason e) override;
  void OnUpstreamDiscarded(int32_t source_id,
                           int64_t media_start_time_ms,
                           int64_t media_end_time_ms) override;
  void OnDownstreamFormatChanged(int32_t source_id,
                                 const util::Format* format,
                                 int32_t trigger,
                                 int64_t media_time_ms) override;

  bool HaveCodecs() const;

 private:
  struct TrackContext {
    TrackContext();
    ~TrackContext();
    DashFrameType frame_type_ = DASH_FRAME_TYPE_INVALID;
    const char* name_ = nullptr;
    int times_selected_ = 0;
    bool has_sample_ = false;
    bool is_eos_ = false;
    // |format_holder_| is a holder populated by the ReadFrame method and is
    // used on the consuming end.
    MediaFormatHolder format_holder_;
    // |sample_holder_| is a holder populated by the ReadFrame method and is
    // used on the consuming end. It represents the next sample that needs to
    // be passed to the API for this track.
    SampleHolder sample_holder_;
    // |upstream_format_| is a copy of the media format determined by the
    // parser on each new chunk.  It is used to initialize the decoder as early
    // as possible.
    std::unique_ptr<const MediaFormat> upstream_format_;
    bool check_pssh_ = true;
    // Order matters. Destroy things in the opposite order in which they
    // were created.
    std::unique_ptr<upstream::DataSourceInterface> data_source_;
    std::unique_ptr<chunk::FormatEvaluatorInterface> format_evaluator_;
    std::unique_ptr<dash::DashChunkSource> chunk_source_;
    std::unique_ptr<chunk::ChunkSampleSource> sample_source_;
    std::unique_ptr<TrackRenderer> renderer_;
    std::unique_ptr<TrackCriteria> track_criteria_;
  };

  // Unless otherwise stated, all private member functions are called by the
  // DashThread's task runner.

  void UnloadImpl();
  void UnloadImplDisabled(TrackRenderer* disabled_renderer);
  void NotifyUnloadWaiter();
  void ScheduleUpdate();
  void Update(bool allow_schedule);
  void UpdateMediaTime();

  // Gets the next track that should have a sample passed to the API. Call
  // this when the current sample has been fully consumed.
  //
  // Returns the TrackContext to read a sample from next; its sample_holder_
  // already contains the sample.
  TrackContext* GetNextSampleTrack();

  // Ensures that each track's SampleHolder has data (if possible).
  //
  // Returns false if all tracks are EOS, true otherwise.
  bool FillTrackSampleHolders();

  // Read a frame from the next renderer (track_renderer).
  int ReadFrame(TrackRenderer* track_renderer,
                MediaFormatHolder* format_holder,
                SampleHolder* sample_holder,
                bool* error_occurred);

  // Waits for the DRM manager to retrieve a playback license
  bool MaybeCheckPssh(TrackContext* track);

  // Populates the various fields in |fi| related to crypto if the sample is
  // encrypted. Copies the data out of sample_holder into scratch_*_ rather
  // than saving pointers/references.
  void PopulateFrameInfoCrypto(struct DashFrameInfo* fi,
                               const SampleHolder& sample_holder);

  // C API methods. Unless otherwise specified, they may only be called
  // from the dash thread. The callbacks provided to C API wrap these in a
  // task that is posted to the dash thread's task runner and then wait for the
  // task's completion.
  int GetVideoCodecSettingsImpl(DashVideoCodecSettings* video_codec_settings);
  int GetAudioCodecSettingsImpl(DashAudioCodecSettings* audio_codec_settings);

  // May be called from any thread
  int CopyFrameImpl(void* buffer, int bufferlen, struct DashFrameInfo* fi);
  int GetCCCodecSettingsImpl(DashCCCodecSettings* codec_settings);
  MediaTimeMs GetFirstTimeMsImpl();
  MediaDurationMs GetDurationMsImpl();
  int SeekImpl(MediaTimeMs time_ms);

  // Disables the current tracks after a rate change.
  void SetPlaybackRateDisableTracks(float target_rate);

  // Enables tracks after a rate change (after tracks were disabled
  // asynchronously by SetPlaybackRateDisableTracks above).
  void SetPlaybackRateEnableTracks(float target_rate,
                                   TrackRenderer* disabled_renderer);

  int GetStreamCountsImpl(int* num_video_streams,
                          int* num_audio_streams,
                          int* num_cc_streams);

  // QOE Dispatch done callback
  void QoePingDispatchDone(bool outcome);

  // Private report function that accepts the full range of qoe video
  // error codes.
  void ReportPlaybackError(qoe::VideoErrorCode code,
                           const std::string& error_string,
                           bool is_fatal);

  void SetState(PlayerState new_state);

  void FormatGiven(TrackContext* track, const MediaFormat* format);

  void NewBandwidthEstimate(base::TimeDelta elapsed,
                            int64_t bytes,
                            int64_t bitrate);

  // This value is used to shift the pts values so that the stream always
  // appears to start at time 0 to the client.
  int64_t GetSampleOffsetMs();

  // Unless stated otherwise, all private member variables are accessed
  // exclusively by the DashThread.
  void* context_;

  PlayerState state_;
  base::TimeDelta seek_position_;
  base::TimeDelta decoder_position_;
  base::TimeDelta reader_position_;
  base::TimeDelta buffered_position_;
  base::TimeTicks elapsed_real_time_;

  TrackContext* current_track_;
  int32_t sample_holder_consumed_;

  base::TimeTicks last_track_summary_;
  base::TimeTicks last_bandwidth_estimate_;
  base::TimeDelta duration_;
  base::TimeDelta initial_time_;
  std::string url_;
  std::set<TrackRenderer*> pending_disable_;

  // is_eos_ is accessed by both the media thread and dash thread.
  std::atomic<bool> is_eos_;

  std::unique_ptr<ManifestFetcher> manifest_fetcher_;
  std::unique_ptr<upstream::AllocatorInterface> allocator_;
  std::unique_ptr<LoadControl> load_control_;
  PlaybackRate playback_rate_;

  std::unique_ptr<upstream::DefaultBandwidthMeter> media_bandwidth_meter_;

  // True when the decoder media time is valid. This is false at initial start
  // and when seeking.
  bool media_time_ready_;
  // Keeps track of the last time we called the decoder's GetMediaTime func.
  base::TimeTicks decoder_media_time_last_call_timestamp_;
  // Keeps track of the last known value from GetMediaTime func.
  int64_t decoder_media_time_last_value_ms_;
  // Keeps track of the last value returned from UpdateMediaTime helper.
  int64_t media_time_last_value_ms_;
  // Holds callback functions into the decoder.
  DashPlayerCallbacks player_callbacks_;
  // license_fetcher_ is accessed by the DrmSessionManager's worker thread.
  // The only other access is done on the API thread in Load() before
  // playback is initiated so no locking is required.
  drm::LicenseFetcher license_fetcher_;
  drm::DrmSessionManager drm_session_manager_;
  std::unique_ptr<qoe::QoeManager> qoe_manager_;
  PlayerAttributes player_attributes_;

  // Scratch space for encryption meta data. Used by PopulateFrameInfoCrypto().
  // Will not be necessary once we move to prAcquireFrame/prReleaseFrame. It is
  // safe to use a single group of scratch buffers because we can assume that
  // the data is done being accessed when another prCopyFrame call comes in.
  std::vector<char> scratch_key_id_;
  std::vector<char> scratch_iv_;
  std::vector<int> scratch_clear_bytes_;
  std::vector<int> scratch_enc_bytes_;

  std::list<TrackContext> tracks_;

  base::WaitableEvent unload_waiter_;
  base::WaitableEvent codec_waiter_;
  base::WaitableEvent playback_rate_waiter_;

  int64_t sample_offset_ms_;
};

}  // namespace ndash

#endif  // NDASH_DASH_THREAD_H_
