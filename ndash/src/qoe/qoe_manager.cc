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

#include "qoe/qoe_manager.h"

namespace ndash {
namespace qoe {

QoeManager::QoeManager() {}

QoeManager::~QoeManager() {}

void QoeManager::SetMediaPos(base::TimeDelta media_pos) {
  last_media_pos_ = media_pos;
}

void QoeManager::ReportUpdate() {
  // TODO(rmrossi): Delegate to client along with last media time.
}

void QoeManager::ReportBuffering() {
  // TODO(rmrossi): Delegate to client along with last media time.
}

void QoeManager::ReportContentLoad(LoadType load_type,
                                   int64_t media_pos_start_ms,
                                   int64_t media_pos_end_ms,
                                   int64_t download_time_ms,
                                   int64_t download_bytes,
                                   int64_t load_start_time_ms,
                                   int64_t load_end_time_ms) {
  // TODO(rmrossi): Delegate to client along with last media time.
}

void QoeManager::ReportManifestParsed(
    std::unique_ptr<base::DictionaryValue> manifest_info,
    int64_t earliest_available_media_pos_ms,
    int64_t latest_available_media_pos_ms,
    int64_t media_pos_ms) {
  // TODO(rmrossi): Delegate to client along with last media time.
}

void QoeManager::ReportPreparing() {
  // TODO(rmrossi): Delegate to client along with last media time.
}

void QoeManager::ReportLoadingManifest() {
  // TODO(rmrossi): Delegate to client along with last media time.
}

void QoeManager::ReportInitializingStream() {
  // TODO(rmrossi): Delegate to client along with last media time.
}

void QoeManager::ReportVideoPlaying() {
  // TODO(rmrossi): Delegate to client along with last media time.
}

void QoeManager::ReportVideoPaused() {
  // TODO(rmrossi): Delegate to client along with last media time.
}

void QoeManager::ReportVideoSeeking() {
  // TODO(rmrossi): Delegate to client along with last media time.
}

void QoeManager::ReportVideoError(VideoErrorCode error_code,
                                  const std::string& details,
                                  bool is_fatal) {
  // TODO(rmrossi): Delegate to client along with last media time.
}

void QoeManager::ReportVideoStopped() {
  // TODO(rmrossi): Delegate to client along with last media time.
}

void QoeManager::ReportVideoEnded() {
  // TODO(rmrossi): Delegate to client along with last media time.
}

}  // namespace qoe
}  // namespace ndash
