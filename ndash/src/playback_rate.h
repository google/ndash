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

#ifndef NDASH_PLAYBACK_RATE_H_
#define NDASH_PLAYBACK_RATE_H_

#include <cmath>

namespace ndash {

class PlaybackRate {
 public:
  explicit PlaybackRate(float rate = 1.0f);
  ~PlaybackRate();

  void set_rate(float rate) { rate_ = rate; }
  float rate() const { return rate_; }
  float abs_rate() const { return std::abs(rate_); }

  bool IsForward() const { return rate_ >= 0; }
  bool IsNormal() const { return rate_ == 1; }
  bool IsTrick() const { return rate_ != 1; }
  float GetMagnitude() const { return std::fabs(rate_); }

 private:
  float rate_;
};

}  // namespace ndash

#endif  // NDASH_PLAYBACK_RATE_H_
