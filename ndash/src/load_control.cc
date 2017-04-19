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

#include "load_control.h"

#include "base/logging.h"

namespace {

constexpr int32_t kDefaultLowWatermarkMs = 30000;
constexpr int32_t kDefaultHighWatermarkMs = 30000;
constexpr double kDefaultLowBufferLoad = 0.9;
constexpr double kDefaultHighBufferLoad = 0.9;

enum WatermarkLevel {
  kAboveHighWatermark = 0,
  kBetweenWatermarks = 1,
  kBelowLowWatermark = 2,
};
}  // namespace

namespace ndash {

LoaderState::LoaderState(int32_t buffer_size_contribution) {
  this->buffer_size_contribution = buffer_size_contribution;
  buffer_state = kAboveHighWatermark;
  loading = false;
  next_load_position_us = -1;
}

LoaderState::~LoaderState() {}

LoadControl::LoadControl(upstream::AllocatorInterface* allocator)
    : LoadControl(allocator, nullptr) {}

LoadControl::LoadControl(upstream::AllocatorInterface* allocator,
                         LoadControlEventListenerInterface* event_listener)
    : LoadControl(allocator,
                  event_listener,
                  kDefaultLowWatermarkMs,
                  kDefaultHighWatermarkMs,
                  kDefaultLowBufferLoad,
                  kDefaultHighBufferLoad) {}

LoadControl::LoadControl(upstream::AllocatorInterface* allocator,
                         LoadControlEventListenerInterface* event_listener,
                         int32_t low_watermark_ms,
                         int32_t high_watermark_ms,
                         double low_buffer_load,
                         double high_buffer_load)
    : allocator_(allocator),
      event_listener_(event_listener),
      low_watermark_us_(low_watermark_ms * 1000L),
      high_watermark_us_(high_watermark_ms * 1000L),
      low_buffer_load_(low_buffer_load),
      high_buffer_load_(high_buffer_load),
      target_buffer_size_(0),
      max_load_start_position_us_(0),
      buffer_state_(0),
      filling_buffers_(false),
      last_loading_notify_(false) {}

LoadControl::~LoadControl() {}

void LoadControl::Register(upstream::LoaderInterface* loader,
                           int32_t buffer_size_contribution) {
  loaders_.insert(loader);
  loader_states_[loader] =
      std::unique_ptr<LoaderState>(new LoaderState(buffer_size_contribution));
  target_buffer_size_ += buffer_size_contribution;
}

void LoadControl::Unregister(upstream::LoaderInterface* loader) {
  DCHECK(loader_states_.find(loader) != loader_states_.end());
  DCHECK(loaders_.find(loader) != loaders_.end());
  loaders_.erase(loader);
  LoaderState* state = loader_states_[loader].get();
  target_buffer_size_ -= state->buffer_size_contribution;
  loader_states_.erase(loader);
  UpdateControlState();
}

void LoadControl::TrimAllocator() {
  allocator_->Trim(target_buffer_size_);
}

upstream::AllocatorInterface* LoadControl::GetAllocator() {
  return allocator_;
}

bool LoadControl::Update(upstream::LoaderInterface* loader,
                         int64_t playback_position_us,
                         int64_t next_load_position_us,
                         bool loading) {
  // Update the loader state.
  int loader_buffer_state =
      GetLoaderBufferState(playback_position_us, next_load_position_us);
  LoaderState* loader_state = loader_states_[loader].get();
  bool loader_state_changed =
      loader_state->buffer_state != loader_buffer_state ||
      loader_state->next_load_position_us != next_load_position_us ||
      loader_state->loading != loading;
  if (loader_state_changed) {
    VLOG(5) << "Loader state change from " << loader_state->buffer_state << "/"
            << loader_state->next_load_position_us << "/"
            << loader_state->loading << " to " << loader_buffer_state << "/"
            << next_load_position_us << "/" << loading;
    loader_state->buffer_state = loader_buffer_state;
    loader_state->next_load_position_us = next_load_position_us;
    loader_state->loading = loading;
  }

  // Update the buffer state.
  size_t current_buffer_size = allocator_->GetTotalBytesAllocated();
  int32_t buffer_state = GetBufferState(current_buffer_size);
  bool buffer_state_changed = buffer_state_ != buffer_state;

  if (buffer_state_changed) {
    VLOG(5) << "Buffer state change from " << buffer_state_ << " to "
            << buffer_state;
    buffer_state_ = buffer_state;
  }

  // If either of the individual states have changed, update the shared control
  // state.
  if (loader_state_changed || buffer_state_changed) {
    UpdateControlState();
  }

  VLOG(1) << "current_buffer_size " << current_buffer_size
          << " target_buffer_size_ " << target_buffer_size_
          << " playback_position_us " << playback_position_us
          << " next_load_position_us " << next_load_position_us
          << " max_load_start_position_us_ " << max_load_start_position_us_;

  return current_buffer_size < (size_t)target_buffer_size_ &&
         next_load_position_us != -1 &&
         next_load_position_us <= max_load_start_position_us_;
}

int32_t LoadControl::GetLoaderBufferState(int64_t playback_position_us,
                                          int64_t next_load_position_us) {
  if (next_load_position_us == -1) {
    return kAboveHighWatermark;
  } else {
    int64_t time_until_next_load_position =
        next_load_position_us - playback_position_us;
    return time_until_next_load_position > high_watermark_us_
               ? kAboveHighWatermark
               : time_until_next_load_position < low_watermark_us_
                     ? kBelowLowWatermark
                     : kBetweenWatermarks;
  }
}

int32_t LoadControl::GetBufferState(int32_t current_buffer_size) {
  float buffer_load = (float)current_buffer_size / target_buffer_size_;
  return buffer_load > high_buffer_load_
             ? kAboveHighWatermark
             : buffer_load < low_buffer_load_ ? kBelowLowWatermark
                                              : kBetweenWatermarks;
}

void LoadControl::UpdateControlState() {
  bool loading = false;
  bool have_next_load_position = false;
  int highest_state = buffer_state_;
  for (auto i = loaders_.begin(); i != loaders_.end(); i++) {
    LoaderState* loader_state = loader_states_[*i].get();
    loading |= loader_state->loading;
    have_next_load_position |= loader_state->next_load_position_us != -1;
    highest_state = std::max(highest_state, loader_state->buffer_state);
  }

  filling_buffers_ =
      !loaders_.empty() && (loading || have_next_load_position) &&
      (highest_state == kBelowLowWatermark ||
       (highest_state == kBetweenWatermarks && filling_buffers_));

  if (filling_buffers_ && !last_loading_notify_) {
    last_loading_notify_ = true;
    NotifyLoadingChanged(true);
  } else if (!filling_buffers_ && last_loading_notify_ && !loading) {
    last_loading_notify_ = false;
    NotifyLoadingChanged(false);
  }

  VLOG(6) << "filling_buffers_ " << filling_buffers_ << " last_loading_notify_ "
          << last_loading_notify_ << " loading " << loading << " num_loaders "
          << loaders_.size();

  max_load_start_position_us_ = -1;
  if (filling_buffers_) {
    for (auto i = loaders_.begin(); i != loaders_.end(); i++) {
      LoaderState* loader_state = loader_states_[*i].get();
      int64_t loader_time = loader_state->next_load_position_us;
      VLOG(6) << "loader_time " << loader_time << " max_load_start"
              << max_load_start_position_us_;
      if (loader_time != -1 && (max_load_start_position_us_ == -1 ||
                                loader_time < max_load_start_position_us_)) {
        max_load_start_position_us_ = loader_time;
      }
    }
  }
}

void LoadControl::NotifyLoadingChanged(bool loading) {
  DLOG(INFO) << __FUNCTION__ << " " << loading;
  if (event_listener_ != nullptr) {
    event_listener_->OnLoadingChanged(loading);
  }
}

}  // namespace ndash
