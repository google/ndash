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

#include "ndash_stream.h"

#include <gflags/gflags.h>
#include <glog/logging.h>

namespace dash {
namespace player {

namespace {

constexpr size_t kMaxFrameBufferLen = 32768;

}  // namespace

int64_t PlayerFrameState::GetMediaTime() {
  if (!valid_pts_) {
    return -1;
  }
  return current_player_audio_pts_microseconds_ / 1000;
}

void PlayerFrameState::UpdateCurrentPlayerAudioPTS(int64_t pts) {
  current_player_audio_pts_microseconds_ = pts;
  valid_pts_ = true;
}

void PlayerFrameState::Flush() {
  pending_flush_ = true;
  valid_pts_ = false;
}

bool PlayerFrameState::IsFlushPending() const {
  return pending_flush_;
}

void PlayerFrameState::ClearPendingFlush() {
  pending_flush_ = false;
}

bool PlayerFrameState::IsValidPTS() const {
  return valid_pts_;
}

NDashStream::NDashStream(struct ndash_handle* player) : player_handle_(player) {
  ndash_set_context(player, this);
}

util::StatusOr<std::unique_ptr<NDashStream>> NDashStream::Make() {
  DashPlayerCallbacks decoder_callbacks;
  decoder_callbacks.get_media_time_ms_func = DoGetMediaTime;
  decoder_callbacks.open_cdm_session_func = DoOpenCdmSession;
  decoder_callbacks.close_cdm_session_func = DoCloseCdmSession;
  decoder_callbacks.fetch_license_func = DoFetchLicense;
  decoder_callbacks.decoder_flush_func = DoDecoderFlush;

  // We pass a null context here because we don't have the instance of
  // NDashStream yet. The constructor for NDashStream will then follow up with a
  // ndash_set_context call.
  struct ndash_handle* player =
      ndash_create(&decoder_callbacks, nullptr /* context */);
  if (!player) {
    return util::Status(util::Code::INTERNAL, "Unable to create dash player");
  }

  return std::unique_ptr<NDashStream>(new NDashStream(player));
}

util::Status NDashStream::Load(const std::string& url) {
  return ndash_load(player_handle_, url.c_str(), 0) == 0
             ? util::Status::OK
             : util::Status(util::Code::INVALID_ARGUMENT,
                            "Could not load dash stream");
}

void NDashStream::Stop() {
  // noop for now
}

bool NDashStream::Seek(int64_t time_ms) {
  return ndash_seek(player_handle_, time_ms) == 0;
}

void NDashStream::SetPlaybackRate(float rate) {
  ndash_set_playback_rate(player_handle_, rate);
}

int64_t NDashStream::GetMediaTime() {
  return player_frame_state_.GetMediaTime();
}

bool NDashStream::GetVideoCodecSettings(
    DashVideoCodecSettings* codec_settings) {
  return ndash_get_video_codec_settings(player_handle_, codec_settings) == 0;
}

bool NDashStream::GetAudioCodecSettings(
    DashAudioCodecSettings* codec_settings) {
  return ndash_get_audio_codec_settings(player_handle_, codec_settings) == 0;
}

size_t NDashStream::ReadFrameInto(std::vector<uint8_t>* frame_buffer,
                                  DashFrameInfo* frame_info) {
  // Assess the available frame buffer data.
  // TODO(rdaum): turn this into an API call for peek instead of abusing
  // CopyFrame
  DashFrameInfo tmp_frame_info;
  ndash_copy_frame(player_handle_, nullptr, 0, &tmp_frame_info);
  size_t available = (size_t)tmp_frame_info.frame_len;
  frame_buffer->resize(available);
  size_t read_size = static_cast<size_t>(ndash_copy_frame(
      player_handle_, frame_buffer->data(), available, frame_info));

  return read_size;
}

void NDashStream::DecoderFlush() {
  player_frame_state_.Flush();
}

MediaTimeMs NDashStream::DoGetMediaTime(void* context) {
  return static_cast<NDashStream*>(context)->GetMediaTime();
}

void NDashStream::DoDecoderFlush(void* context) {
  static_cast<NDashStream*>(context)->DecoderFlush();
}

DashCdmStatus NDashStream::CloseCdmSession(const char* session_id, int len) {
  // We don't suport encrypted content.
  return DashCdmStatus::DASH_CDM_FAILURE;
}

DashCdmStatus NDashStream::OpenCdmSession(char** session_id, size_t* len) {
  // We don't suport encrypted content.
  return DashCdmStatus::DASH_CDM_FAILURE;
}

DashCdmStatus NDashStream::FetchLicense(const char* session_id,
                                        int session_id_len,
                                        const char* pssh,
                                        int pssh_len) {
  // We don't suport encrypted content.
  return DashCdmStatus::DASH_CDM_FAILURE;
}

DashCdmStatus NDashStream::DoOpenCdmSession(void* context,
                                            char** session_id,
                                            size_t* len) {
  return static_cast<NDashStream*>(context)->OpenCdmSession(session_id, len);
}

DashCdmStatus NDashStream::DoFetchLicense(void* context,
                                          const char* session_id,
                                          size_t session_id_len,
                                          const char* pssh,
                                          size_t pssh_len) {
  return static_cast<NDashStream*>(context)->FetchLicense(
      session_id, session_id_len, pssh, pssh_len);
}

DashCdmStatus NDashStream::DoCloseCdmSession(void* context,
                                             const char* session_id,
                                             size_t len) {
  return static_cast<NDashStream*>(context)->CloseCdmSession(session_id, len);
}

NDashStream::~NDashStream() {
  ndash_destroy(player_handle_);
}

}  // namespace player
}  // namespace dash
