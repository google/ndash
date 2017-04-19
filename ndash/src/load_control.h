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

// TODO(rmrossi): It appears the time-based high/low watermark logic
// in this class does not influence the decision to allow load or not
// on update calls.  We should revisit this logic. It also appears
// to not let any one loader load more than 1 segment ahead of another
// which is probably not what we want. (cdrolle@ suggested we simply
// remove the time-based logic.)

#ifndef NDASH_LOAD_CONTROL_H_
#define NDASH_LOAD_CONTROL_H_

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>

#include "upstream/allocator.h"
#include "upstream/loader.h"

namespace ndash {

// Interface definition for a callback to be notified of LoadControl events.
class LoadControlEventListenerInterface {
 public:
  LoadControlEventListenerInterface(){};
  virtual ~LoadControlEventListenerInterface(){};

  // Invoked when the control transitions from a loading to a draining state,
  // or vice versa.
  // loading Whether the control is now in a loading state.
  virtual void OnLoadingChanged(bool loading) = 0;
};

struct LoaderState {
  explicit LoaderState(int32_t buffer_size_contribution);
  ~LoaderState();

  int32_t buffer_size_contribution;
  int32_t buffer_state;
  bool loading;
  int64_t next_load_position_us;
};

// A LoadControl implementation that allows loads to continue in a sequence
// that prevents any loader from getting too far ahead or behind any of the
// other loaders.
//
// Loads are scheduled so as to fill the available buffer space as rapidly as
// possible. Once the duration of buffered media and the buffer utilization
// both exceed respective thresholds, the control switches to a draining state
// during which no loads are permitted to start. The control reverts back to
// the loading state when either the duration of buffered media or the buffer
// utilization fall below respective thresholds.
class LoadControl {
 public:
  // Constructs a new instance, using the kDefault* constants.
  // allocator The Allocator used by the loader.
  explicit LoadControl(upstream::AllocatorInterface* allocator);

  // Constructs a new instance, using the kDefault* constants.
  // allocator The Allocator used by the loader. May be null if delivery of
  //   events is not required.
  // eventListener A listener of events. May be null if delivery of events is
  //   not required.
  LoadControl(upstream::AllocatorInterface* allocator,
              LoadControlEventListenerInterface* eventListener);

  // Constructs a new instance.
  //
  // allocator The Allocator used by the loader.
  // eventListener A listener of events. May be null if delivery of events is
  //   not required.
  // lowWatermarkMs The minimum duration of media that can be buffered for the
  //   control to be in the draining state. If less media is buffered, then the
  //   control will transition to the filling state.
  // highWatermarkMs The minimum duration of media that can be buffered for the
  //   control to transition from filling to draining.
  // lowBufferLoad The minimum fraction of the buffer that must be utilized for
  //   the control to be in the draining state. If the utilization is lower,
  //   then the control will transition to the filling state.
  // highBufferLoad The minimum fraction of the buffer that must be utilized
  //   for the control to transition from the loading state to the draining
  //   state.
  //
  LoadControl(upstream::AllocatorInterface* allocator,
              LoadControlEventListenerInterface* event_listener,
              int32_t low_watermark_ms,
              int32_t high_watermark_ms,
              double low_buffer_load,
              double high_buffer_load);

  ~LoadControl();

  void Register(upstream::LoaderInterface* loader,
                int32_t buffer_size_contribution);
  void Unregister(upstream::LoaderInterface* loader);
  void TrimAllocator();
  upstream::AllocatorInterface* GetAllocator();

  bool Update(upstream::LoaderInterface* loader,
              int64_t playback_position_us,
              int64_t next_load_position_us,
              bool loading);

 private:
  upstream::AllocatorInterface* allocator_;
  // TODO(rmrossi): Consider eliminating the set and iterate over map keys
  // instead.
  std::set<upstream::LoaderInterface*> loaders_;
  std::map<upstream::LoaderInterface*, std::unique_ptr<LoaderState>>
      loader_states_;
  LoadControlEventListenerInterface* event_listener_;

  int64_t low_watermark_us_;
  int64_t high_watermark_us_;
  double low_buffer_load_;
  double high_buffer_load_;

  int32_t target_buffer_size_;
  int64_t max_load_start_position_us_;
  int32_t buffer_state_;
  bool filling_buffers_;
  bool last_loading_notify_;

  int32_t GetLoaderBufferState(int64_t playbackPositionUs,
                               int64_t nextLoadPositionUs);
  int32_t GetBufferState(int32_t currentBufferSize);
  void UpdateControlState();
  void NotifyLoadingChanged(bool loading);
};

}  // namespace ndash

#endif  // NDASH_LOAD_CONTROL_H_
