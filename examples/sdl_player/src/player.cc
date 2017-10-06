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

#include "player.h"

#include <chrono>

#include <SDL.h>
#include <SDL_mixer.h>
#include <SDL_thread.h>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <ndash.h>
#include <values.h>

#include "frame_source_queue.h"
#include "ndash_stream.h"

namespace dash {
namespace player {

DEFINE_int32(audio_buffer_size, 1024, "Audio buffer size");

namespace {

constexpr std::chrono::milliseconds kMaxAudioDriftTime(20);

constexpr int kPlaybackRates[] = {-240, -120, -60, -15, -4, 1,
                                  4,    15,   60,  120, 240};

constexpr int kNormalRateIndex = 5;
constexpr int kMinRateIndex = 0;
constexpr int kMaxRateIndex = 10;
constexpr int kMinWindowWidth = 640;

typedef std::chrono::high_resolution_clock::time_point TimePoint;
typedef std::chrono::high_resolution_clock::duration Duration;

}  // namespace

Player::Player(std::unique_ptr<NDashStream> dash_stream,
               WindowPtr window,
               RendererPtr renderer)
    : window_(std::move(window)),
      renderer_(std::move(renderer)),
      dash_stream_(std::move(dash_stream)),
      running_(false),
      paused_(false),
      start_time_ms_(-1),
      playback_rate_index_(kNormalRateIndex) {}

util::StatusOr<std::unique_ptr<FrameSourceQueue>> Player::CreateFrameQueue() {
  util::StatusOr<std::unique_ptr<FrameSourceQueue>> frame_source_result =
      FrameSourceQueue::Make(dash_stream_.get());
  if (!frame_source_result.ok()) {
    LOG(ERROR) << "Can't create frame source queue";
    return frame_source_result.status();
  }
  return std::move(frame_source_result.ConsumeValueOrDie());
}

void Player::SDLAudioCallback(void* userdata, uint8_t* stream, int len) {
  Player* player = static_cast<Player*>(userdata);
  player->PerformAudio(stream, static_cast<size_t>(len));
}

void Player::PerformAudio(uint8_t* audio_buffer, size_t num_bytes) {
  int64_t audio_pts_microseconds;
  size_t amount_read = frame_source_queue_->ReadAudio(num_bytes, audio_buffer,
                                                      &audio_pts_microseconds);

  if (amount_read < num_bytes) {
    LOG(ERROR) << "Audio buffer underflow";
    return;
  }

  dash_stream_->GetPlayerFrameState().UpdateCurrentPlayerAudioPTS(
      audio_pts_microseconds);
}

util::StatusOr<std::unique_ptr<Player>> Player::Make(
    const std::string& dash_url) {
  av_register_all();

  auto dash_stream_result = NDashStream::Make();
  if (!dash_stream_result.ok()) {
    LOG(ERROR) << "Unable to create DASH player: "
               << dash_stream_result.status();
    return dash_stream_result.status();
  }

  auto dash_stream = dash_stream_result.ConsumeValueOrDie();
  auto load_status = dash_stream->Load(dash_url);
  if (!load_status.ok()) {
    return load_status;
  }

  DashVideoCodecSettings video_codec_settings;
  if (!dash_stream->GetVideoCodecSettings(&video_codec_settings)) {
    return util::Status(util::Code::UNAVAILABLE,
                        "Unable to detect video codec settings");
  }

  DashAudioCodecSettings audio_codec_settings;
  if (!dash_stream->GetAudioCodecSettings(&audio_codec_settings)) {
    return util::Status(util::Code::UNAVAILABLE,
                        "Unable to detect audio codec settings");
  }

  // For our playback window, we will use the reported width/height from
  // the video codec settings to establish an aspect ratio. But our window
  // will be bigger if the reported values are lower than a reasonable size.
  // All video frames thereafter will be scaled into that window.
  size_t video_width = video_codec_settings.width;
  size_t video_height = video_codec_settings.height;
  size_t window_width = video_width;
  size_t window_height = video_height;
  double aspect_ratio = (double)video_height / (double)video_width;
  if (window_width < kMinWindowWidth) {
    window_width = kMinWindowWidth;
    window_height = kMinWindowWidth * aspect_ratio;
  }

  WindowPtr window{
      SDL_CreateWindow("player", SDL_WINDOWPOS_UNDEFINED,
                       SDL_WINDOWPOS_UNDEFINED, window_width, window_height,
                       SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE),
      [](SDL_Window* w) { SDL_DestroyWindow(w); }};
  if (!window) {
    return util::Status(util::Code::UNAVAILABLE,
                        "Unable to open SDL window for output");
  }

  RendererPtr renderer{
      SDL_CreateRenderer(window.get(), -1,
                         SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC),
      [](SDL_Renderer* r) { SDL_DestroyRenderer(r); }};
  if (!renderer) {
    return util::Status(util::Code::UNAVAILABLE,
                        "Unable to open SDL renderer for output");
  }

  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
  SDL_RenderSetLogicalSize(renderer.get(), window_width, window_height);
  SDL_SetRenderDrawColor(renderer.get(), 0, 0, 0, 255);
  SDL_RenderClear(renderer.get());
  SDL_RenderPresent(renderer.get());

  std::unique_ptr<Player> player(new Player(
      std::move(dash_stream), std::move(window), std::move(renderer)));

  SDL_AudioSpec wanted_spec;
  wanted_spec.channels = audio_codec_settings.num_channels;
  wanted_spec.freq = audio_codec_settings.sample_rate;
  wanted_spec.format = AUDIO_S16SYS;
  wanted_spec.silence = 0;
  wanted_spec.samples = FLAGS_audio_buffer_size;
  wanted_spec.callback = SDLAudioCallback;
  wanted_spec.userdata = player.get();

  if (SDL_OpenAudio(&wanted_spec, &player->GetAudioSpec()) < 0) {
    return util::Status(util::Code::UNAVAILABLE, SDL_GetError());
  }

  return std::move(player);
}

void Player::Start() {
  int current_rate_index = kNormalRateIndex;
  running_ = true;

  while (running_) {
    // Handle seek requests.
    if (start_time_ms_ != -1) {
      dash_stream_->Seek(start_time_ms_);
      start_time_ms_ = -1;
    }

    // Handle rate change.
    if (current_rate_index != playback_rate_index_) {
      current_rate_index = playback_rate_index_;
      dash_stream_->SetPlaybackRate(kPlaybackRates[current_rate_index]);
    }

    // Flush was requested, start with a new frame source queue.
    if (dash_stream_->GetPlayerFrameState().IsFlushPending()) {
      frame_source_queue_ = CreateFrameQueue().ConsumeValueOrDie();
      dash_stream_->GetPlayerFrameState().ClearPendingFlush();
    }

    AVSampleFormat av_sample_format;
    switch (audio_spec_.format) {
      case AUDIO_U8:
        av_sample_format = AV_SAMPLE_FMT_U8;
        break;
      case AUDIO_S16:
        av_sample_format = AV_SAMPLE_FMT_S16;
        break;
      default:
        LOG(ERROR) << "SDL audio output format not supported";
        return;
    }

    frame_decoder_thread_ = frame_source_queue_->DecoderLoop(
        audio_spec_.freq, av_sample_format, audio_spec_.channels);

    // NOTE: Has to be performed on the same thread that initialized the SDL
    // renderer/texture.
    RenderLoop();
    frame_decoder_thread_.join();
  }
}

void Player::RenderLoop() {
  TimePoint last_frame_render_time = std::chrono::high_resolution_clock::now();
  int64_t last_pts_microseconds = 0;

  if (playback_rate_index_ == kNormalRateIndex) {
    // TODO(rmrossi): Assume no audio for rates != 1 for now, but this should
    // use stream count to make this decision later.
    SDL_PauseAudio(0);
  }

  while (running_) {
    // Check the keyboard for quit/pause, etc.
    SDL_Event event;
    SDL_PollEvent(&event);
    switch (event.type) {
      case SDL_QUIT:
        Stop();
        return;
      case SDL_KEYDOWN:
        if (event.key.keysym.sym == SDLK_q) {
          Stop();
          return;
        } else if (event.key.keysym.sym == SDLK_SPACE) {
          if (playback_rate_index_ != kNormalRateIndex) {
            // Space bar brings us back to normal rate while tricking.
            playback_rate_index_ = kNormalRateIndex;
            frame_source_queue_->Stop();
            return;
          }
          paused_ = !paused_;
          SDL_PauseAudio(paused_);
          last_pts_microseconds = 0;
        } else if (event.key.keysym.sym == SDLK_RIGHT) {
          // Seek forward.
          if (!dash_stream_->GetPlayerFrameState().IsValidPTS()) {
            continue;
          }
          SDL_PauseAudio(1);
          int64_t now_ms =
              dash_stream_->GetPlayerFrameState().GetAudioPTSMicroseconds() /
              1000;
          start_time_ms_ = now_ms + 30000;
          // TODO(rmrossi) Use the duration from dashlib to prevent seeking
          // beyond max here.
          frame_source_queue_->Stop();
          return;
        } else if (event.key.keysym.sym == SDLK_LEFT) {
          // Seek backward.
          if (!dash_stream_->GetPlayerFrameState().IsValidPTS()) {
            continue;
          }
          SDL_PauseAudio(1);
          int64_t now_ms =
              dash_stream_->GetPlayerFrameState().GetAudioPTSMicroseconds() /
              1000;
          start_time_ms_ = now_ms - 30000;
          if (start_time_ms_ < 0) {
            start_time_ms_ = 0;
          }
          frame_source_queue_->Stop();
          return;
        } else if (event.key.keysym.sym == SDLK_RIGHTBRACKET) {
          // Trick forward.
          if (!dash_stream_->GetPlayerFrameState().IsValidPTS()) {
            continue;
          }
          SDL_PauseAudio(1);
          if (playback_rate_index_ < kNormalRateIndex) {
            playback_rate_index_ = kNormalRateIndex;
          }
          playback_rate_index_++;
          if (playback_rate_index_ > kMaxRateIndex) {
            playback_rate_index_ = kMaxRateIndex;
          }
          frame_source_queue_->Stop();
          return;
        } else if (event.key.keysym.sym == SDLK_LEFTBRACKET) {
          // Trick backward.
          if (!dash_stream_->GetPlayerFrameState().IsValidPTS()) {
            continue;
          }
          SDL_PauseAudio(1);
          if (playback_rate_index_ > kNormalRateIndex) {
            playback_rate_index_ = kNormalRateIndex;
          }
          playback_rate_index_--;
          if (playback_rate_index_ < kMinRateIndex) {
            playback_rate_index_ = kMinRateIndex;
          }
          frame_source_queue_->Stop();
          return;
        }
        break;
    }

    // Actually pop and render frames.
    if (!paused_) {
      int64_t video_pts_microseconds;
      AVFramePtr output_frame =
          move(frame_source_queue_->PopVideoFrame(&video_pts_microseconds));

      Duration render_time(std::chrono::microseconds(0));

      if (output_frame) {
        if (playback_rate_index_ != kNormalRateIndex) {
          // TODO(rmrossi): When not playing at normal rate, assume no audio
          // track will give us pts updates. Later, we should use available
          // stream counts instead of making this assumption.
          dash_stream_->GetPlayerFrameState().UpdateCurrentPlayerAudioPTS(
              video_pts_microseconds);
        }
        if (!last_pts_microseconds) {
          last_pts_microseconds = video_pts_microseconds;
        } else {
          // Calculate how long to wait until rendering the frame by looking at
          // the previous frame's pts and comparing to the next, and then
          // subtract the time it actually took to render the frame.
          Duration frame_delay = std::chrono::microseconds(
              abs(video_pts_microseconds - last_pts_microseconds));

          // Calculate how far off the audio is from the video, we will use this
          // to calculate if we need to drop some frames to catch up.
          Duration audio_drift(
              std::chrono::microseconds(dash_stream_->GetPlayerFrameState()
                                            .GetAudioPTSMicroseconds()) -
              std::chrono::microseconds(video_pts_microseconds));

          frame_delay = frame_delay - render_time;

          // If we've fallen too far behind the audio, skip frames until we
          // reach it.
          if (audio_drift - frame_delay > kMaxAudioDriftTime) {
            LOG(WARNING)
                << "Audio behind by "
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       audio_drift - frame_delay)
                       .count()
                << "ms, dropping frame";
            last_frame_render_time = std::chrono::high_resolution_clock::now();
            last_pts_microseconds = 0;
            continue;
          }
          last_pts_microseconds = video_pts_microseconds;

          std::this_thread::sleep_for(
              frame_delay / std::abs(kPlaybackRates[playback_rate_index_]));
        }
        uint8_t* planes[3]{output_frame->data[0], output_frame->data[1],
                           output_frame->data[2]};
        int pitches[3]{output_frame->linesize[0], output_frame->linesize[1],
                       output_frame->linesize[2]};

        if (!texture_ || prev_frame_width != output_frame->width ||
            prev_frame_height != output_frame->height) {
          TexturePtr texture{
              SDL_CreateTexture(renderer_.get(), SDL_PIXELFORMAT_YV12,
                                SDL_TEXTUREACCESS_STREAMING,
                                output_frame->width, output_frame->height),
              [](SDL_Texture* t) { SDL_DestroyTexture(t); }};
          texture_ = std::move(texture);
          prev_frame_width = output_frame->width;
          prev_frame_height = output_frame->height;
        }

        // Window size may have changed. Scale accordingly.
        SDL_Rect dest_rect;
        dest_rect.x = 0;
        dest_rect.y = 0;
        int win_w;
        int win_h;
        SDL_GetWindowSize(window_.get(), &win_w, &win_h);
        SDL_RenderSetLogicalSize(renderer_.get(), win_w, win_h);

        double aspect_ratio = (double) output_frame->height / (double)output_frame->width;
        win_h = (int)((double)win_w * aspect_ratio);
        dest_rect.w = win_w;
        dest_rect.h = win_h;

        SDL_UpdateYUVTexture(texture_.get(), nullptr, planes[0], pitches[0],
                             planes[1], pitches[1], planes[2], pitches[2]);
        SDL_RenderClear(renderer_.get());
        SDL_RenderCopy(renderer_.get(), texture_.get(), nullptr, &dest_rect);
        SDL_RenderPresent(renderer_.get());

        // Calculate how long it took to actually render the last frame and
        // subtract that from the frame delay the next time around.
        render_time =
            std::chrono::high_resolution_clock::now() - last_frame_render_time;
        last_frame_render_time = std::chrono::high_resolution_clock::now();
      }
    }
  }
}

void Player::Stop() {
  SDL_PauseAudio(1);
  running_ = false;
  frame_source_queue_->Stop();
}

Player::~Player() {
  SDL_PauseAudio(1);
  SDL_CloseAudio();
}

}  // namespace player
}  // namespace dash
