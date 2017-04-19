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

#include "sample_source_track_renderer.h"

#include "base/bind.h"
#include "base/logging.h"

namespace ndash {

SampleSourceTrackRenderer::SampleSourceTrackRenderer(
    SampleSourceInterface* source)
    : source_(source->Register()),
      source_is_enabled_(false),
      source_is_ready_(false),
      duration_us_(0) {}

SampleSourceTrackRenderer::~SampleSourceTrackRenderer() {
  source_->Release();
}

bool SampleSourceTrackRenderer::DoPrepare(int64_t position_us) {
  if (!source_->Prepare(position_us)) {
    return false;
  }

  int64_t duration_us = source_->GetDurationUs();
  if (duration_us == util::kMatchLongestUs) {
    LOG(WARNING)
        << "Track duration was kMatchLongestUs but we only support one track";
    duration_us = util::kUnknownTimeUs;
  }
  duration_us_ = duration_us;
  return true;
}

bool SampleSourceTrackRenderer::OnEnabled(const TrackCriteria* track_criteria,
                                          int64_t position_us,
                                          bool joining) {
  source_->Enable(track_criteria, position_us);
  source_is_enabled_ = true;
  return OnDiscontinuity(position_us);
}

bool SampleSourceTrackRenderer::SeekTo(base::TimeDelta position) {
  DCHECK(source_is_enabled_);
  source_->SeekToUs(position.InMicroseconds());
  bool error_occurred = false;
  CheckForDiscontinuity(position.InMicroseconds(), &error_occurred);
  if (error_occurred) {
    return false;
  }
  return true;
}

bool SampleSourceTrackRenderer::IsSourceReady() {
  return source_is_ready_;
}

void SampleSourceTrackRenderer::Buffer(int64_t position_us) {
  DCHECK(source_is_enabled_);
  source_is_ready_ = source_->ContinueBuffering(position_us);
}

SampleSourceReaderInterface::ReadResult SampleSourceTrackRenderer::ReadFrame(
    int64_t position_us,
    MediaFormatHolder* format_holder,
    SampleHolder* sample_holder,
    bool* error_occurred) {
  DCHECK(source_is_enabled_);

  position_us = CheckForDiscontinuity(position_us, error_occurred);
  if (*error_occurred) {
    return SampleSourceReaderInterface::NOTHING_READ;
  }

  // NOTE: At this point in the ExoPlayer implementation, we would call into
  // the subclass implementation to do some work.  In our case, we consume from
  // the sample queue so we can deliver it back to the pull reader.
  if (IsSourceReady()) {
    return source_->ReadData(position_us, format_holder, sample_holder);
  } else {
    return SampleSourceReaderInterface::NOTHING_READ;
  }
}

int64_t SampleSourceTrackRenderer::GetBufferedPositionUs() {
  DCHECK(source_is_enabled_);
  return source_->GetBufferedPositionUs();
}

int64_t SampleSourceTrackRenderer::GetDurationUs() {
  return duration_us_;
}

bool SampleSourceTrackRenderer::CanContinueBuffering() {
  DCHECK(source_is_enabled_);
  return source_->CanContinueBuffering();
}

bool SampleSourceTrackRenderer::OnDisabled(
    const base::Closure* disable_done_callback) {
  if (!TrackRenderer::OnDisabled(disable_done_callback)) {
    return false;
  }
  base::Closure callback =
      base::Bind(&TrackRenderer::DisableDone, base::Unretained(this),
                 base::Unretained(source_));
  source_->Disable(&callback);
  source_is_enabled_ = false;
  return true;
}

bool SampleSourceTrackRenderer::OnReleased() {
  DCHECK(!source_is_enabled_);
  source_->Release();
  return true;
}

bool SampleSourceTrackRenderer::IsEnded() {
  // TODO(rmrossi): Figure out whether we need this.
  return false;
}

bool SampleSourceTrackRenderer::IsReady() {
  // TODO(rmrossi): Figure out whether we need this.
  return true;
}

bool SampleSourceTrackRenderer::OnDiscontinuity(int64_t position_us) {
  // TODO(rmrossi): Figure out whether we need this.
  return true;
}

// Private methods.

int64_t SampleSourceTrackRenderer::CheckForDiscontinuity(int64_t position_us,
                                                         bool* error_occurred) {
  *error_occurred = false;
  int64_t discontinuity_position_us = source_->ReadDiscontinuity();
  if (discontinuity_position_us !=
      SampleSourceReaderInterface::NO_DISCONTINUITY) {
    *error_occurred = OnDiscontinuity(discontinuity_position_us);
    return discontinuity_position_us;
  }
  return position_us;
}

}  // namespace ndash
