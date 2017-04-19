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

#ifndef FRAME_SOURCE_QUEUE_H_
#define FRAME_SOURCE_QUEUE_H_

#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <queue>

#include <gflags/gflags.h>
#include <glog/logging.h>
#include <values.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

#include "ndash.h"
#include "ndash_stream.h"
#include "util/byte_buffer.h"
#include "util/statusor.h"

namespace dash {
namespace player {

typedef std::unique_ptr<AVFrame, std::function<void(AVFrame*)>> AVFramePtr;

typedef std::unique_ptr<AVCodecContext, std::function<void(AVCodecContext*)>>
    AVCodecContextPtr;
typedef std::unique_ptr<AVCodecParameters,
                        std::function<void(AVCodecParameters*)>>
    AVCodecParametersPtr;

class FrameSourceQueue {
 public:
  FrameSourceQueue(NDashStream* dash_stream,
                   AVCodecParametersPtr video_codec_par,
                   AVCodecContextPtr video_codec_context,
                   AVCodecParametersPtr audio_codec_par,
                   AVCodecContextPtr audio_codec_context);

  static util::StatusOr<std::unique_ptr<FrameSourceQueue>> Make(
      NDashStream* dash_stream);

  bool PullFrames(int out_sample_rate,
                  AVSampleFormat out_sample_format,
                  int audio_output_num_channels);

  void Stop();

  size_t ReadAudio(size_t buffer_size, uint8_t* audio_buffer, int64_t* pts);

  AVFramePtr PopVideoFrame(int64_t* pts_microseconds);

  int GetWidth() const;

  int GetHeight() const;

  int GetAudioSampleRate() const;

  int GetNumAudioChannels() const;

  void Flush();

  AVSampleFormat GetAudioSampleFormat() const;

  // Spawn the decoder loop on a separate thread.
  std::thread DecoderLoop(int freq, AVSampleFormat sample_format, int channels);

 private:
  void PushVideoFrame(AVFramePtr frame);

  std::atomic_bool running_;

  NDashStream* dash_stream_;

  AVCodecParametersPtr video_codec_par_;
  AVCodecContextPtr video_codec_context_;
  AVCodecParametersPtr audio_codec_par_;
  AVCodecContextPtr audio_codec_context_;

  std::mutex video_queue_mutex_;
  std::mutex frame_queue_mutex_;
  std::queue<AVFramePtr> video_output_frames;

  util::PtsByteBuffer audio_buffer_;
};

}  // namespace player
}  // namespace dash

#endif  // FRAME_SOURCE_QUEUE_H_
