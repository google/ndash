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

#ifndef NDASH_STREAM_H_
#define NDASH_STREAM_H_

#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <ndash.h>

#include "util/byte_buffer.h"
#include "util/statusor.h"

namespace dash {
namespace player {

class AVContextWrapper;

class PlayerFrameState {
 public:
  PlayerFrameState()
      : current_player_audio_pts_microseconds_(0),
        pending_flush_(true),
        valid_pts_(false) {}

  int64_t GetMediaTime();

  void UpdateCurrentPlayerAudioPTS(int64_t pts);

  void Flush();
  bool IsFlushPending() const;
  void ClearPendingFlush();
  bool IsValidPTS() const;

  int64_t GetAudioPTSMicroseconds() const {
    return current_player_audio_pts_microseconds_;
  }

 private:
  int64_t current_player_audio_pts_microseconds_;
  std::atomic_bool pending_flush_;
  std::atomic_bool valid_pts_;
};

class NDashStream {
 public:
  static util::StatusOr<std::unique_ptr<NDashStream>> Make();

  ~NDashStream();

  ndash_handle* player() const { return player_handle_; }

  util::Status Load(const std::string& url);

  void Stop();
  bool Seek(int64_t time_ms);
  void SetPlaybackRate(float rate);

  bool GetVideoCodecSettings(DashVideoCodecSettings* codec_settings);
  bool GetAudioCodecSettings(DashAudioCodecSettings* codec_settings);

  // Read a frame into the given format context buffer.
  size_t ReadFrameInto(std::vector<uint8_t>* frame_buffer,
                       DashFrameInfo* frame_info);

  PlayerFrameState& GetPlayerFrameState() { return player_frame_state_; }

 protected:
  // Returns the current playback position time in milliseconds.
  int64_t GetMediaTime();

  void DecoderFlush();
  DashCdmStatus OpenCdmSession(char** session_id, size_t* len);
  DashCdmStatus FetchLicense(const char* session_id,
                             int session_id_len,
                             const char* pssh,
                             int pssh_len);
  DashCdmStatus CloseCdmSession(const char* session_id, int len);

 private:
  explicit NDashStream(ndash_handle* player);

  static int64_t DoGetMediaTime(void* context);
  static void DoDecoderFlush(void* context);
  static DashCdmStatus DoOpenCdmSession(void* context,
                                        char** session_id,
                                        size_t* len);
  static DashCdmStatus DoFetchLicense(void* context,
                                      const char* session_id,
                                      size_t session_id_len,
                                      const char* pssh,
                                      size_t pssh_len);
  static DashCdmStatus DoCloseCdmSession(void* context,
                                         const char* session_id,
                                         size_t len);

  ndash_handle* player_handle_;

  PlayerFrameState player_frame_state_;
};

}  // namespace player
}  // namespace dash

#endif  // NDASH_STREAM_H_
