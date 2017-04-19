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

#include "frame_source_queue.h"

#include <utility>

#include <ndash.h>
#include <gflags/gflags.h>
#include <glog/logging.h>

extern "C" {
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>
}

namespace dash {
namespace player {

namespace {

std::string AvError(int averr) {
  char errstr[60];
  av_make_error_string(errstr, sizeof(errstr), averr);

  return std::string(errstr);
}

AVFramePtr DecodeFramePacket(AVPacket* packet, AVCodecContext* codec_context) {
  // Send the packet into the codec context.
  int send_packet = avcodec_send_packet(codec_context, packet);
  if (send_packet != 0) {
    LOG(ERROR) << "could not feed packet: " << AvError(send_packet);
    return nullptr;
  }

  // Decode it into a frame.
  AVFramePtr frame{av_frame_alloc(), [](AVFrame* f) { av_frame_free(&f); }};

  int recv_result = avcodec_receive_frame(codec_context, frame.get());
  if (recv_result != 0) {
    LOG(ERROR) << "could not receive packet: " << AvError(recv_result);
    return nullptr;
  }

  return frame;
}

constexpr AVRational kMicroseconds = {1, 1000000};

// TODO(rdaum): This is a constant right now but should be returned by libdash.
constexpr AVRational kTimebase = {1, 90000};

}  // namespace

FrameSourceQueue::FrameSourceQueue(NDashStream* dash_stream,
                                   AVCodecParametersPtr video_codec_par,
                                   AVCodecContextPtr video_codec_context,
                                   AVCodecParametersPtr audio_codec_par,
                                   AVCodecContextPtr audio_codec_context)
    : dash_stream_(dash_stream),
      video_codec_par_(std::move(video_codec_par)),
      video_codec_context_(std::move(video_codec_context)),
      audio_codec_par_(std::move(audio_codec_par)),
      audio_codec_context_(std::move(audio_codec_context)) {}

util::StatusOr<std::unique_ptr<FrameSourceQueue>> FrameSourceQueue::Make(
    NDashStream* dash_stream) {
  AVCodecParametersPtr audio_codec_par{avcodec_parameters_alloc(),
                                       [](AVCodecParameters* parameters) {
                                         avcodec_parameters_free(&parameters);
                                       }};
  DashAudioCodecSettings audio_codec_settings;
  if (!dash_stream->GetAudioCodecSettings(&audio_codec_settings)) {
    return util::Status(util::Code::UNAVAILABLE,
                        "Unable to get audio codec settings");
  }
  audio_codec_par->sample_rate = audio_codec_settings.sample_rate;
  audio_codec_par->block_align = audio_codec_settings.blockalign;
  audio_codec_par->bit_rate = audio_codec_settings.bitrate;
  audio_codec_par->channels = audio_codec_settings.num_channels;

  switch (audio_codec_settings.channel_layout) {
    case DashChannelLayout::kChannelLayoutMono:
      audio_codec_par->channel_layout = 1;
      break;
    case DashChannelLayout::kChannelLayoutStereo:
      audio_codec_par->channel_layout = 3;
      break;
    default:
      return util::Status(util::Code::UNAVAILABLE,
                          "Unsupported channel layout");
  }

  switch (audio_codec_settings.sample_format) {
    case DashSampleFormat::kSampleFormatPlanarF32:
      audio_codec_par->format = AV_SAMPLE_FMT_FLTP;
      break;
    case DashSampleFormat::kSampleFormatF32:
      audio_codec_par->format = AV_SAMPLE_FMT_FLT;
      break;
    case DashSampleFormat::kSampleFormatS16:
      audio_codec_par->format = AV_SAMPLE_FMT_S16;
      break;
    case DashSampleFormat::kSampleFormatPlanarS16:
      audio_codec_par->format = AV_SAMPLE_FMT_S16P;
      break;
    case DashSampleFormat::kSampleFormatPlanarS32:
      audio_codec_par->format = AV_SAMPLE_FMT_S32P;
      break;
    case DashSampleFormat::kSampleFormatS32:
      audio_codec_par->format = AV_SAMPLE_FMT_S32;
      break;
    case DashSampleFormat::kSampleFormatU8:
      audio_codec_par->format = AV_SAMPLE_FMT_U8;
      break;
    default:
      return util::Status(util::Code::UNAVAILABLE,
                          "Unsupported sample format");
  }

  // Translate from libdash's codec id to the equivalent in avcodec.
  // Some values are not translated that could be, but in reality libdash only
  // really supports AAC.
  AVCodecID audio_avcodec_id;
  switch (audio_codec_settings.audio_codec) {
    case DASH_AUDIO_MPEG_LAYER123:
      audio_avcodec_id = AV_CODEC_ID_MP3;
      break;
    case DASH_AUDIO_AAC:
      audio_avcodec_id = AV_CODEC_ID_AAC;
      break;
    case DASH_AUDIO_AC3:
      audio_avcodec_id = AV_CODEC_ID_AC3;
      break;
    case DASH_AUDIO_DTS:
      audio_avcodec_id = AV_CODEC_ID_DTS;
      break;
    case DASH_AUDIO_NONE:
      return util::Status(util::Code::INVALID_ARGUMENT, "No audio stream");
    case DASH_AUDIO_EAC3:
    case DASH_AUDIO_UNSUPPORTED:
    default:
      return util::Status(util::Code::INVALID_ARGUMENT,
                          "Unknown or unsupported audio codec");
  }
  AVCodec* audio_codec = avcodec_find_decoder(audio_avcodec_id);

  AVCodecParametersPtr video_codec_par{avcodec_parameters_alloc(),
                                       [](AVCodecParameters* parameters) {
                                         avcodec_parameters_free(&parameters);
                                       }};
  DashVideoCodecSettings video_codec_settings;
  if (!dash_stream->GetVideoCodecSettings(&video_codec_settings)) {
    return util::Status(util::Code::UNAVAILABLE,
                        "Unable to get video codec settings");
  }

  video_codec_par->width = video_codec_settings.width;
  video_codec_par->height = video_codec_settings.height;

  // Translate from libdash's codec id to the equivalent in avcodec.
  // Some values are not translated that could be, but in reality libdash only
  // really supports H264.
  AVCodecID video_avcodec_id;
  switch (video_codec_settings.video_codec) {
    case DASH_VIDEO_H264:
      video_avcodec_id = AV_CODEC_ID_H264;
      break;
    case DASH_VIDEO_NONE:
      return util::Status(util::Code::INVALID_ARGUMENT, "Missing video codec");
    default:
    case DASH_VIDEO_UNSUPPORTED:
      return util::Status(util::Code::INVALID_ARGUMENT,
                          "Unknown or unsupported video codec");
  }
  AVCodec* video_codec = avcodec_find_decoder(video_avcodec_id);

  AVCodecContextPtr video_codec_context{
      avcodec_alloc_context3(video_codec),
      [](AVCodecContext* ctx) { avcodec_free_context(&ctx); }};
  video_codec_context->refcounted_frames = 0;
  LOG(INFO) << "Video codec: " << avcodec_get_name(video_codec->id);
  int codec_open =
      avcodec_open2(video_codec_context.get(), video_codec, nullptr);
  if (codec_open != 0) {
    return ::util::Status(util::Code::UNAVAILABLE,
                          "Could not open video codec");
  }

  AVCodecContextPtr audio_codec_context{
      avcodec_alloc_context3(audio_codec),
      [](AVCodecContext* ctx) { avcodec_free_context(&ctx); }};

  audio_codec_context->refcounted_frames = 0;
  LOG(INFO) << "Audio codec: " << avcodec_get_name(audio_codec->id);
  codec_open = avcodec_open2(audio_codec_context.get(), audio_codec, nullptr);
  if (codec_open != 0) {
    return ::util::Status(util::Code::UNAVAILABLE,
                          "Could not open audio codec");
  }

  return std::unique_ptr<FrameSourceQueue>(new FrameSourceQueue(
      dash_stream, std::move(video_codec_par), std::move(video_codec_context),
      std::move(audio_codec_par), std::move(audio_codec_context)));
}

std::thread FrameSourceQueue::DecoderLoop(int freq,
                                          AVSampleFormat sample_format,
                                          int channels) {
  running_ = true;
  return std::thread([this, freq, sample_format, channels]() {
    PullFrames(freq, sample_format, channels);
  });
}

bool FrameSourceQueue::PullFrames(int out_sample_rate,
                                  AVSampleFormat audio_output_sample_format,
                                  int audio_output_num_channels) {
  std::unique_ptr<SwrContext, std::function<void(SwrContext*)>> swr(
      swr_alloc_set_opts(nullptr, audio_codec_par_->channel_layout,
                         audio_output_sample_format, out_sample_rate,
                         audio_codec_par_->channel_layout,
                         (AVSampleFormat)audio_codec_par_->format,
                         audio_codec_par_->sample_rate, 0, nullptr),
      [](SwrContext* s) { swr_free(&s); });

  int swr_init_result = swr_init(swr.get());
  if (swr_init_result < 0) {
    LOG(ERROR) << "Failure to initialize audio resampling context: "
               << swr_init_result;
    return false;
  }

  AVPacket packet;
  while (running_) {
    std::lock_guard<std::mutex> queue_lock(frame_queue_mutex_);

    DashFrameInfo frame_info;
    av_init_packet(&packet);

    std::vector<uint8_t> frame_buffer;
    size_t frame_bytes_size =
        dash_stream_->ReadFrameInto(&frame_buffer, &frame_info);
    if (!frame_bytes_size) {
      continue;
    }
    packet.data = frame_buffer.data();
    packet.pts = frame_info.pts;
    packet.duration = frame_info.duration;
    packet.size = (int)frame_bytes_size;

    if (frame_info.type == DASH_FRAME_TYPE_VIDEO) {
      AVFramePtr frame = DecodeFramePacket(&packet, video_codec_context_.get());
      if (!frame) {
        continue;
      }
      if (running_) {
        PushVideoFrame(std::move(frame));
      }
    } else if (frame_info.type == DASH_FRAME_TYPE_AUDIO) {
      AVFramePtr frame = DecodeFramePacket(&packet, audio_codec_context_.get());
      if (!frame) {
        continue;
      }

      // Resample the audio to the desired output sample rate and format.
      /* compute destination number of samples */
      int64_t dst_nb_samples = av_rescale_rnd(
          swr_get_delay(swr.get(), frame->sample_rate) + frame->nb_samples,
          out_sample_rate, frame->sample_rate, AV_ROUND_UP);

      uint8_t** dst_data;
      int dst_linesize;
      int samples_ret = av_samples_alloc_array_and_samples(
          &dst_data, &dst_linesize, audio_output_num_channels, dst_nb_samples,
          audio_output_sample_format, 1);
      if (samples_ret < 0) {
        LOG(ERROR) << "av_samples_alloc_array_and_samples error: "
                   << AvError(samples_ret);
        return false;
      }
      int ret =
          swr_convert(swr.get(), dst_data, dst_nb_samples,
                      (const uint8_t**)frame->extended_data, frame->nb_samples);
      if (ret < 0) {
        LOG(ERROR) << "swr_convert error: " << AvError(ret);
        return false;
      }

      int dst_bufsize =
          av_samples_get_buffer_size(&dst_linesize, audio_output_num_channels,
                                     ret, audio_output_sample_format, 1);

      if (dst_bufsize < 0) {
        LOG(ERROR) << "av_samples_get_buffer_size error: "
                   << AvError(dst_bufsize);
        return false;
      }

      int64_t pts_microseconds =
          av_rescale_q(frame->pts, kTimebase, kMicroseconds);

      audio_buffer_.Write(dst_bufsize, dst_data[0], pts_microseconds);

      av_free(dst_data[0]);
    }
  }
  LOG(INFO) << "Done decoding";
  return true;
}

AVFramePtr FrameSourceQueue::PopVideoFrame(int64_t* pts_microseconds) {
  std::lock_guard<std::mutex> queue_lock(video_queue_mutex_);
  if (video_output_frames.empty()) {
    return nullptr;
  }

  AVFramePtr frame = std::move(video_output_frames.front());
  *pts_microseconds = av_rescale_q(frame->pts, kTimebase, kMicroseconds);
  video_output_frames.pop();
  return frame;
}

int FrameSourceQueue::GetWidth() const {
  return video_codec_par_->width;
}

int FrameSourceQueue::GetHeight() const {
  return video_codec_par_->height;
}

int FrameSourceQueue::GetAudioSampleRate() const {
  return audio_codec_par_->sample_rate;
}

int FrameSourceQueue::GetNumAudioChannels() const {
  return audio_codec_par_->channels;
}

AVSampleFormat FrameSourceQueue::GetAudioSampleFormat() const {
  return (AVSampleFormat)audio_codec_par_->format;
}

void FrameSourceQueue::PushVideoFrame(AVFramePtr frame) {
  std::lock_guard<std::mutex> queue_lock(video_queue_mutex_);
  video_output_frames.emplace(std::move(frame));
}

size_t FrameSourceQueue::ReadAudio(size_t buffer_size,
                                   uint8_t* audio_buffer,
                                   int64_t* pts) {
  return audio_buffer_.Read(buffer_size, audio_buffer, pts);
}

void FrameSourceQueue::Stop() {
  running_ = false;
}

void FrameSourceQueue::Flush() {
  //  std::lock_guard<std::mutex> frame_queue_lock(frame_queue_mutex_);
  //  std::lock_guard<std::mutex> queue_lock2(video_queue_mutex_);

  avcodec_flush_buffers(video_codec_context_.get());
  avcodec_flush_buffers(audio_codec_context_.get());

  audio_buffer_.Flush();
  std::queue<AVFramePtr> empty;
  std::swap(video_output_frames, empty);
}

}  // namespace player
}  // namespace dash
