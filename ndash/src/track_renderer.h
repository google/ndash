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

#ifndef NDASH_MEDIA_TRACK_RENDERER_H_
#define NDASH_MEDIA_TRACK_RENDERER_H_

#include "base/callback.h"
#include "base/synchronization/lock.h"
#include "media_clock.h"
#include "media_format_holder.h"
#include "sample_holder.h"
#include "sample_source_reader.h"
#include "util/util.h"

namespace ndash {

struct TrackCriteria;

// Renders a single component of media.
//
// Internally, a renderer's lifecycle is managed by the owning DashThread.
// The player will transition its renderers through various states as the
// overall playback state changes.
// Unless stated otherwise, methods in this class should be called only by
// the buffering (DashThread) thread.
class TrackRenderer {
 public:
  enum RendererState {
    // The renderer has been released and should not be used.
    RELEASED = -1,
    // The renderer has not yet been prepared.
    UNPREPARED = 0,
    // The renderer has completed necessary preparation. Preparation may
    // include, for example, reading the header of a media file to determine
    // the track format and duration. The renderer should not hold scarce or
    // expensive system resources (e.g. media decoders) and should not be
    // actively buffering media data when in this state.
    PREPARED = 1,
    // The renderer is enabled. It should either be ready to be started, or be
    // actively working towards this state (e.g. a renderer in this state will
    // typically hold any resources that it requires, such as media decoders,
    // and will have buffered or be buffering any media data that is required
    // to start playback.
    ENABLED = 2,
    // The renderer is started. Calls to doSomeWork() should cause the media to
    // be rendered.
    STARTED = 3,
  };

  TrackRenderer();
  virtual ~TrackRenderer();

  // Prepares the renderer. This method is non-blocking, and hence it may be
  // necessary to call it more than once in order to transition the renderer
  // into the prepared state.
  //
  // position_us The player's current playback position.
  // Returns false if the renderer could not transition to a prepared state,
  // true otherwise.
  bool Prepare(int64_t position_us);

  // Enable the renderer for a specified track.
  //
  // track_criteria The criteria for track selection.
  // position_us The player's current position.
  // joining Whether this renderer is being enabled to join an ongoing playback.
  // Returns false if an error occurred, true otherwise.
  bool Enable(const TrackCriteria* track_criteria,
              int64_t position_us,
              bool joining);

  // Starts the renderer, meaning that calls to doSomeWork() will cause the
  // track to be rendered.
  // Returns false if an error occurred, true otherwise.
  bool Start();

  // Disable the renderer. Disabling a renderer is an asynchronous operation.
  // disable_done_callback: an optional callback that will be invoked once
  // the track has been disabled.
  // Returns false if an error occurred, true otherwise.
  bool Disable(const base::Closure* disable_done_callback = nullptr);

  // Releases the renderer.
  // Returns false if an error occurred, true otherwise.
  bool Release();

  // Stops the renderer.
  // Returns false if an error occurred, true otherwise.
  bool Stop();

  // Invoked to make progress on the loading/frame production end of the
  // renderer when the renderer is in the ENABLED or STARTED states.
  // This method may be called when the renderer is in the following states:
  // ENABLED, STARTED
  // position_us The current media time in microseconds, measured at the start
  // of the current iteration of the rendering loop.
  virtual void Buffer(int64_t position_us) = 0;

  // Invoked by the API thread to read frame data when the renderer is
  // in the ENABLED or STARTED states.
  //
  // This method should return quickly, and should not block if the renderer is
  // currently unable to make any useful progress.
  //
  // This method may be called when the renderer is in the following states:
  // ENABLED, STARTED
  //
  // position_us The current media time in microseconds, measured at the start
  // of the current iteration of the rendering loop.
  // Sets error_occurred to true if an error occurred.  False otherwise.
  // Returns one of the ReadResult constants defined in
  // SampleSourceReaderInterface.
  virtual SampleSourceReaderInterface::ReadResult ReadFrame(
      int64_t position_us,
      MediaFormatHolder* format_holder,
      SampleHolder* sample_holder,
      bool* error_occurred) = 0;

  // Returns the current state of the renderer.
  RendererState GetState();

  // Whether the renderer is ready for the DashThread instance to transition to
  // ENDED. The player will make this transition as soon as true is
  // returned by all of its TrackRenderers.
  // This method may be called when the renderer is in the following states:
  // ENABLED, STARTED
  virtual bool IsEnded() = 0;

  // Whether the renderer is able to immediately render media from the current
  // position.
  //
  // If the renderer is in the STARTED state then returning true indicates that
  // the renderer has everything that it needs to continue playback. Returning
  // false indicates that the player should pause until the renderer is ready.
  //
  // If the renderer is in the ENABLED state then returning true indicates
  // that the renderer is ready for playback to be started. Returning false
  // indicates that it is not.
  //
  // This method may be called when the renderer is in the following states:
  // ENABLED, STARTED
  // Returns true if the renderer is ready to render media. False otherwise.
  virtual bool IsReady() = 0;

  // Returns an error that's preventing the renderer from making progress or
  // buffering more data at this point in time.
  // Returns false if an error occurred, true otherwise.
  virtual bool CanContinueBuffering() = 0;

  // Returns the duration of the media being rendered.
  //
  // This method may be called when the renderer is in the following states:
  // PREPARED, ENABLED, STARTED
  //
  // Return the duration of the track in microseconds, or kMatchLongestUs if
  // the track's duration should match that of the longest track whose duration
  // is known, kUnknownTimeUs if the duration is not known.
  virtual int64_t GetDurationUs() = 0;

  // Returns an estimate of the absolute position in microseconds up to which
  // data is buffered.
  //
  // This method may be called when the renderer is in the following states:
  // ENABLED, STARTED
  //
  // Return an estimate of the absolute position in microseconds up to which
  // data is buffered, or kEndOfTrackUs if the track is fully buffered,
  // kUnknownTimeUs if no estimate is available.
  virtual int64_t GetBufferedPositionUs() = 0;

  // Seeks to a specified time in the track.
  //
  // This method may be called when the renderer is in the following states:
  // ENABLED
  //
  // position_us The desired playback position in microseconds.
  // Returns false if an error occurred, true otherwise.
  virtual bool SeekTo(base::TimeDelta position) = 0;

  // Called by the consumer (API) thread to determine whether this
  // track is ready to have frames read from it.
  virtual bool IsSourceReady() = 0;

  // This method should be set to be the disable done callback on the
  // source reader by subclass implementations when the reader is disabled.
  void DisableDone(SampleSourceReaderInterface* source);

 private:
  RendererState state_;
  base::Closure disable_done_callback_;

 protected:
  // If the renderer advances its own playback position then this method
  // returns a corresponding MediaClock. If provided, the player will use the
  // returned MediaClock as its source of time during playback. A player may
  // have at most one renderer that returns a MediaClock from this method.
  virtual MediaClockInterface* GetMediaClock();

  // Invoked to make progress when the renderer is in the UNPREPARED state. This
  // method will be called repeatedly until true is returned.
  // This method should return quickly, and should not block if the renderer is
  // currently unable to make any useful progress.
  // position_us The player's current playback position.
  // Returns true if the renderer was prepared. False otherwise.
  virtual bool DoPrepare(int64_t position_us) = 0;

  // Called when the renderer is enabled.
  // The default implementation is a no-op.
  // track_criteria The criteria for track selection.
  // position_us The player's current position.
  // joining Whether this renderer is being enabled to join an ongoing playback.
  // Returns false if an error occurred, true otherwise.
  virtual bool OnEnabled(const TrackCriteria* track_criteria,
                         int64_t position_us,
                         bool joining);

  // Called when the renderer is started.
  // The default implementation is a no-op.
  // Returns false if an error occurred, true otherwise.
  virtual bool OnStarted();

  // Called when the renderer is stopped.
  // The default implementation is a no-op.
  // Returns false if an error occurred, true otherwise.
  virtual bool OnStopped();

  // Called when the renderer is disabled.
  // Subclass implementations should call the parent implementation to manage
  // the optional done callback invocation.
  // Returns false if an error occurred, true otherwise.
  virtual bool OnDisabled(const base::Closure* disable_done_callback = nullptr);

  // Called when the renderer is released.
  // The default implementation is a no-op.
  // Returns false if an error occurred, true otherwise.
  virtual bool OnReleased();
};

}  // namespace ndash

#endif  // NDASH_MEDIA_TRACK_RENDERER_H_
