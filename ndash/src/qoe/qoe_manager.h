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

#ifndef NDASH_QOE_QOE_MANAGER_H_
#define NDASH_QOE_QOE_MANAGER_H_

#include <memory>
#include <mutex>

#include "base/time/time.h"
#include "base/values.h"

namespace ndash {
namespace qoe {

enum class VideoErrorCode {
  UNKNOWN_ERROR,
  INVALID_DATA,
  SOFT_TIMEOUT,
  NETWORK_CONNECTION_ERROR,
  MANIFEST_FETCH_ERROR,
  MEDIA_FETCH_ERROR,
  FRAMEWORK_MEDIA_PLAYER_AUDIO_INIT_ERROR,
  FRAMEWORK_MEDIA_PLAYER_VIDEO_INIT_ERROR,
  FRAMEWORK_MEDIA_PLAYER_PLAYBACK_ERROR,
  FRAMEWORK_MEDIA_PLAYER_DRM_ERROR
};

enum class LoadType {
  UNKNOWN,
  MANIFEST,
  AUDIO,
  VIDEO,
  CLOSED_CAPTIONS,
  LICENSE
};

// Handles reporting of Quality of Experience (QoE) metrics.
//
// Events are reported to QoeManager from elsewhere in the DASH player.
class QoeManager {
 public:
  // TODO(rmrossi): Allow the client to pass in an implementation of a
  // callback. We will delegate the reported events to it along with the last
  // known media time.  For now, the events go nowhere.
  QoeManager();
  ~QoeManager();

  // Set the last known media position that should be reported from here on.
  void SetMediaPos(base::TimeDelta media_pos);

  // Report various player events.
  void ReportUpdate();
  void ReportBuffering();
  void ReportContentLoad(LoadType load_type,
                         int64_t media_pos_start_ms,
                         int64_t media_pos_end_ms,
                         int64_t download_time_ms,
                         int64_t download_bytes,
                         int64_t load_start_time_ms,
                         int64_t load_end_time_ms);
  void ReportManifestParsed(
      std::unique_ptr<base::DictionaryValue> manifest_info,
      int64_t earliest_available_media_pos_ms,
      int64_t latest_available_media_pos_ms,
      int64_t media_pos_ms);
  void ReportPreparing();
  void ReportLoadingManifest();
  void ReportInitializingStream();
  void ReportVideoPlaying();
  void ReportVideoPaused();
  void ReportVideoSeeking();
  void ReportVideoError(VideoErrorCode error_code,
                        const std::string& details,
                        bool is_fatal);
  void ReportVideoStopped();
  void ReportVideoEnded();

 private:
  base::TimeDelta last_media_pos_;
};

}  // namespace qoe
}  // namespace ndash

#endif  // NDASH_QOE_QOE_MANAGER_H_
