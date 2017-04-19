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

#ifndef NDASH_MEDIA_SAMPLE_SOURCE_TRACK_RENDERER_H_
#define NDASH_MEDIA_SAMPLE_SOURCE_TRACK_RENDERER_H_

#include "track_renderer.h"

#include <atomic>
#include <cstdint>
#include <memory>

#include "base/synchronization/lock.h"
#include "sample_source.h"
#include "sample_source_reader.h"

namespace ndash {

// Base class for TrackRenderer implementations that render samples obtained
// from a SampleSource.
class SampleSourceTrackRenderer : public TrackRenderer {
 private:
  SampleSourceReaderInterface* source_;
  std::atomic<bool> source_is_enabled_;
  // The source is 'ready' if it has content to be consumed.
  std::atomic<bool> source_is_ready_;

  int64_t duration_us_;

 public:
  // source An upstream source from which the renderer can obtain samples.
  explicit SampleSourceTrackRenderer(SampleSourceInterface* source);
  ~SampleSourceTrackRenderer() override;

  // Called by DashThread's task runner to trigger load events and produce
  // data into the sample queue.
  void Buffer(int64_t position_us) override;

  // Called by the API thread to read frames out of the sample
  // queue.
  SampleSourceReaderInterface::ReadResult ReadFrame(
      int64_t position_us,
      MediaFormatHolder* format_holder,
      SampleHolder* sample_holder,
      bool* error_occurred) override;

 protected:
  bool DoPrepare(int64_t positionUs) override;
  bool OnEnabled(const TrackCriteria* track_criteria,
                 int64_t position_us,
                 bool joining) override;
  bool SeekTo(base::TimeDelta position) override;
  bool IsSourceReady() override;
  int64_t GetBufferedPositionUs() override;
  int64_t GetDurationUs() override;
  bool CanContinueBuffering() override;
  bool OnDisabled(
      const base::Closure* disable_done_callback = nullptr) override;
  bool OnReleased() override;

  bool IsEnded() override;
  bool IsReady() override;

  // Invoked when a discontinuity is encountered. Also invoked when the
  // renderer is enabled, for convenience.
  //
  // position_us The playback position after the discontinuity, or the position
  // at which the renderer is being enabled.
  // Returns false if an error occurs handling the discontinuity, true
  // otherwise.
  bool OnDiscontinuity(int64_t position_us);

 private:
  int64_t CheckForDiscontinuity(int64_t positionUs, bool* error_occurred);
};

}  // namespace ndash

#endif  // NDASH_MEDIA_SAMPLE_SOURCE_TRACK_RENDERER_H_
