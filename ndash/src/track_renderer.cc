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

#include "track_renderer.h"

#include "base/bind.h"
#include "base/logging.h"

namespace ndash {

TrackRenderer::TrackRenderer() : state_(UNPREPARED) {}

TrackRenderer::~TrackRenderer() {}

MediaClockInterface* TrackRenderer::GetMediaClock() {
  return nullptr;
}

// Returns the current state of the renderer.
TrackRenderer::RendererState TrackRenderer::GetState() {
  // TODO(rmrossi): Add DCHECK_CURRENTLY_ON(BUFFERING_THREAD) when available.
  return state_;
}

bool TrackRenderer::Prepare(int64_t positionUs) {
  // TODO(rmrossi): Add DCHECK_CURRENTLY_ON(BUFFERING_THREAD) when available.
  DCHECK(state_ == UNPREPARED);
  state_ = DoPrepare(positionUs) ? PREPARED : UNPREPARED;
  return state_ == PREPARED;
}

bool TrackRenderer::Enable(const TrackCriteria* track_criteria,
                           int64_t position_us,
                           bool joining) {
  // TODO(rmrossi): Add DCHECK_CURRENTLY_ON(BUFFERING_THREAD) when available.
  DCHECK(state_ == PREPARED);
  CHECK(track_criteria != nullptr);
  state_ = ENABLED;
  return OnEnabled(track_criteria, position_us, joining);
}

bool TrackRenderer::Start() {
  // TODO(rmrossi): Add DCHECK_CURRENTLY_ON(BUFFERING_THREAD) when available.
  DCHECK(state_ == ENABLED);
  state_ = STARTED;
  return OnStarted();
}

bool TrackRenderer::Stop() {
  // TODO(rmrossi): Add DCHECK_CURRENTLY_ON(BUFFERING_THREAD) when available.
  DCHECK(state_ == STARTED);
  state_ = ENABLED;
  return OnStopped();
}

bool TrackRenderer::Disable(const base::Closure* disable_done_callback) {
  // TODO(rmrossi): Add DCHECK_CURRENTLY_ON(BUFFERING_THREAD) when available.
  DCHECK(state_ == ENABLED);
  state_ = PREPARED;
  return OnDisabled(disable_done_callback);
}

bool TrackRenderer::Release() {
  // TODO(rmrossi): Add DCHECK_CURRENTLY_ON(BUFFERING_THREAD) when available.
  DCHECK(state_ != ENABLED && state_ != STARTED && state_ != RELEASED);
  state_ = RELEASED;
  return OnReleased();
}

bool TrackRenderer::OnEnabled(const TrackCriteria* track_criteria,
                              int64_t position_us,
                              bool joining) {
  return true;
}

bool TrackRenderer::OnStarted() {
  return true;
}

bool TrackRenderer::OnStopped() {
  return true;
}

bool TrackRenderer::OnDisabled(const base::Closure* disable_done_callback) {
  if (disable_done_callback) {
    disable_done_callback_ = *disable_done_callback;
  }
  return true;
}

void TrackRenderer::DisableDone(SampleSourceReaderInterface* source) {
  if (!disable_done_callback_.is_null()) {
    disable_done_callback_.Run();
    disable_done_callback_.Reset();
  }
}

bool TrackRenderer::OnReleased() {
  return true;
}

}  // namespace ndash
