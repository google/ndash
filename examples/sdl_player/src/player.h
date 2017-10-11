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

#ifndef PLAYER_H_
#define PLAYER_H_

#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>

#include <SDL.h>

#include "ndash_stream.h"
#include "util/statusor.h"

namespace dash {
namespace player {

class FrameSourceQueue;

typedef std::unique_ptr<SDL_Window, std::function<void(SDL_Window*)>> WindowPtr;
typedef std::unique_ptr<SDL_Renderer, std::function<void(SDL_Renderer*)>>
    RendererPtr;
typedef std::unique_ptr<SDL_Texture, std::function<void(SDL_Texture*)>>
    TexturePtr;

class Player {
 public:
  static util::StatusOr<std::unique_ptr<Player>> Make(
      const std::string& dash_url);

  ~Player();

  void Start();

  void Stop();

  SDL_AudioSpec& GetAudioSpec() { return audio_spec_; }

 private:
  Player(std::unique_ptr<NDashStream> dash_stream,
         WindowPtr window,
         RendererPtr renderer);

  util::StatusOr<std::unique_ptr<FrameSourceQueue>> CreateFrameQueue();
  void RenderLoop();

  static void SDLAudioCallback(void* userdata, uint8_t* stream, int len);
  void PerformAudio(uint8_t* audio_buffer, size_t num_bytes);

  WindowPtr window_;
  RendererPtr renderer_;
  TexturePtr texture_;

  std::unique_ptr<NDashStream> dash_stream_;
  std::unique_ptr<FrameSourceQueue> frame_source_queue_;

  std::atomic_bool running_;

  bool paused_;

  SDL_AudioSpec audio_spec_;

  std::thread frame_decoder_thread_;
  int64_t start_time_ms_;
  int playback_rate_index_;

  // Used to detect resolution changes which require a newly sized texture.
  int prev_frame_width = -1;
  int prev_frame_height = -1;
};

}  // namespace player
}  // namespace dash

#endif  // P"LAYER_H_
