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

#include "extractor/stream_parser_extractor.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "drm/drm_init_data.h"
#include "drm/drm_session_manager.h"
#include "drm/scheme_init_data.h"
#include "extractor/chunk_index.h"
#include "extractor/extractor_input.h"
#include "extractor/extractor_output.h"
#include "extractor/track_output.h"
#include "media_format.h"
#include "mp4/audio_decoder_config.h"
#include "mp4/channel_layout.h"
#include "mp4/media_log.h"
#include "mp4/media_track.h"
#include "mp4/media_tracks.h"
#include "mp4/size.h"
#include "mp4/stream_parser.h"
#include "mp4/stream_parser_buffer.h"
#include "mp4/video_decoder_config.h"
#include "ndash.h"
#include "util/mime_types.h"
#include "util/util.h"

namespace ndash {
namespace extractor {

namespace {
constexpr size_t kReadBufferSize = 4096;

DashSampleFormat ChromiumSampleFormatToNDash(media::SampleFormat format) {
  switch (format) {
    case media::SampleFormat::kSampleFormatU8:
      return DashSampleFormat::kSampleFormatU8;
    case media::SampleFormat::kSampleFormatS16:
      return DashSampleFormat::kSampleFormatS16;
    case media::SampleFormat::kSampleFormatS32:
      return DashSampleFormat::kSampleFormatS32;
    case media::SampleFormat::kSampleFormatF32:
      return DashSampleFormat::kSampleFormatF32;
    case media::SampleFormat::kSampleFormatPlanarS16:
      return DashSampleFormat::kSampleFormatPlanarS16;
    case media::SampleFormat::kSampleFormatPlanarF32:
      return DashSampleFormat::kSampleFormatPlanarF32;
    case media::SampleFormat::kSampleFormatPlanarS32:
      return DashSampleFormat::kSampleFormatPlanarS32;
    case media::SampleFormat::kSampleFormatS24:
      return DashSampleFormat::kSampleFormatS24;
    default:
      return DashSampleFormat::kSampleFormatUnknown;
  }
}

DashChannelLayout ChromiumChannelLayoutToNDash(media::ChannelLayout layout) {
  switch (layout) {
    case media::ChannelLayout::CHANNEL_LAYOUT_NONE:
      return DashChannelLayout::kChannelLayoutNone;
    case media::ChannelLayout::CHANNEL_LAYOUT_MONO:
      return DashChannelLayout::kChannelLayoutMono;
    case media::ChannelLayout::CHANNEL_LAYOUT_STEREO:
      return DashChannelLayout::kChannelLayoutStereo;
    case media::ChannelLayout::CHANNEL_LAYOUT_2_1:
      return DashChannelLayout::kChannelLayout2_1;
    case media::ChannelLayout::CHANNEL_LAYOUT_SURROUND:
      return DashChannelLayout::kChannelLayoutSurround;
    case media::ChannelLayout::CHANNEL_LAYOUT_4_0:
      return DashChannelLayout::kChannelLayout4_0;
    case media::ChannelLayout::CHANNEL_LAYOUT_2_2:
      return DashChannelLayout::kChannelLayout2_2;
    case media::ChannelLayout::CHANNEL_LAYOUT_QUAD:
      return DashChannelLayout::kChannelLayoutQuad;
    case media::ChannelLayout::CHANNEL_LAYOUT_5_0:
      return DashChannelLayout::kChannelLayout5_0;
    case media::ChannelLayout::CHANNEL_LAYOUT_5_1:
      return DashChannelLayout::kChannelLayout5_1;
    case media::ChannelLayout::CHANNEL_LAYOUT_5_0_BACK:
      return DashChannelLayout::kChannelLayout5_0_Back;
    case media::ChannelLayout::CHANNEL_LAYOUT_5_1_BACK:
      return DashChannelLayout::kChannelLayout5_1_Back;
    case media::ChannelLayout::CHANNEL_LAYOUT_7_0:
      return DashChannelLayout::kChannelLayout7_0;
    case media::ChannelLayout::CHANNEL_LAYOUT_7_1:
      return DashChannelLayout::kChannelLayout7_1;
    case media::ChannelLayout::CHANNEL_LAYOUT_7_1_WIDE:
      return DashChannelLayout::kChannelLayout7_1_Wide;
    case media::ChannelLayout::CHANNEL_LAYOUT_STEREO_DOWNMIX:
      return DashChannelLayout::kChannelLayoutStereoDownmix;
    case media::ChannelLayout::CHANNEL_LAYOUT_2POINT1:
      return DashChannelLayout::kChannelLayout2Point1;
    case media::ChannelLayout::CHANNEL_LAYOUT_3_1:
      return DashChannelLayout::kChannelLayout3_1;
    case media::ChannelLayout::CHANNEL_LAYOUT_4_1:
      return DashChannelLayout::kChannelLayout4_1;
    case media::ChannelLayout::CHANNEL_LAYOUT_6_0:
      return DashChannelLayout::kChannelLayout6_0;
    case media::ChannelLayout::CHANNEL_LAYOUT_6_0_FRONT:
      return DashChannelLayout::kChannelLayout6_0_Front;
    case media::ChannelLayout::CHANNEL_LAYOUT_HEXAGONAL:
      return DashChannelLayout::kChannelLayoutHexagonal;
    case media::ChannelLayout::CHANNEL_LAYOUT_6_1:
      return DashChannelLayout::kChannelLayout6_1;
    case media::ChannelLayout::CHANNEL_LAYOUT_6_1_BACK:
      return DashChannelLayout::kChannelLayout6_1_Back;
    case media::ChannelLayout::CHANNEL_LAYOUT_6_1_FRONT:
      return DashChannelLayout::kChannelLayout6_1_Front;
    case media::ChannelLayout::CHANNEL_LAYOUT_7_0_FRONT:
      return DashChannelLayout::kChannelLayout7_0_Front;
    case media::ChannelLayout::CHANNEL_LAYOUT_7_1_WIDE_BACK:
      return DashChannelLayout::kChannelLayout7_1_WideBack;
    case media::ChannelLayout::CHANNEL_LAYOUT_OCTAGONAL:
      return DashChannelLayout::kChannelLayoutOctagonal;
    case media::ChannelLayout::CHANNEL_LAYOUT_DISCRETE:
      return DashChannelLayout::kChannelLayoutDiscrete;
    case media::ChannelLayout::CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC:
      return DashChannelLayout::kChannelLayoutStereoAndKeyboardMic;
    case media::ChannelLayout::CHANNEL_LAYOUT_4_1_QUAD_SIDE:
      return DashChannelLayout::kChannelLayout4_1_QuadSide;
    default:
      return DashChannelLayout::kChannelLayoutUnsupported;
  }
}

}  // namespace

StreamParserExtractor::StreamParserExtractor(
    drm::DrmSessionManagerInterface* drm_session_manager,
    std::unique_ptr<StreamParser> parser,
    scoped_refptr<media::MediaLog> media_log,
    bool reverse_frames)
    : parser_(std::move(parser)),
      media_log_(media_log),
      track_map_(),
      drm_session_manager_(drm_session_manager),
      reverse_frames_(reverse_frames) {
  parser_->Init(
      base::Bind(&StreamParserExtractor::InitF, base::Unretained(this)),
      base::Bind(&StreamParserExtractor::NewConfigF, base::Unretained(this)),
      base::Bind(&StreamParserExtractor::NewBuffersF, base::Unretained(this)),
      true,
      base::Bind(&StreamParserExtractor::EncryptedMediaInitDataF,
                 base::Unretained(this)),
      base::Bind(&StreamParserExtractor::NewMediaSegmentF,
                 base::Unretained(this)),
      base::Bind(&StreamParserExtractor::EndMediaSegmentF,
                 base::Unretained(this)),
      base::Bind(&StreamParserExtractor::NewSIDX, base::Unretained(this)),

      media_log_);
}

StreamParserExtractor::~StreamParserExtractor() {}

void StreamParserExtractor::Init(ExtractorOutputInterface* output) {
  output_ = output;
}

bool StreamParserExtractor::Sniff(ExtractorInputInterface* input) {
  // We aren't implementing the ExtractorInput buffering required to make this
  // work, so just be optimistic and always return true.
  return true;
}

int StreamParserExtractor::Read(ExtractorInputInterface* input,
                                int64_t* seek_position) {
  uint8_t buf[kReadBufferSize];

  ssize_t result = input->Read(buf, sizeof(buf));

  if (result > 0) {
    if (parser_->Parse(buf, result)) {
      return RESULT_CONTINUE;
    } else {
      // Probably not really appropriate, but we'll figure out better error
      // handling later
      return RESULT_IO_ERROR;
    }
  } else if (result == 0) {
    // Nothing to give the parser but presumably we can still send it data
    return RESULT_CONTINUE;
  } else if (result == RESULT_END_OF_INPUT) {
    return RESULT_END_OF_INPUT;
  } else {
    return RESULT_IO_ERROR;
  }
}

void StreamParserExtractor::Seek() {
  is_seeking_ = true;
  parser_->Flush();
}

void StreamParserExtractor::Release() {
  output_ = nullptr;
}

void StreamParserExtractor::InitF(const StreamParser::InitParameters& params) {
  // TODO(adewhurst): This will only be called at most once per
  //                  Mp4StreamParser::Init() call, so it might potentially do
  //                  the wrong thing if one of the chunks has a different
  //                  length (only the last one?). Otherwise, stuff shouldn't
  //                  change within a representation (each representation
  //                  within a period gets its own StreamParserExtractor), so
  //                  that should be OK.
  VLOG(5) << "InitParameters received: " << params.detected_audio_track_count
          << " audio tracks; " << params.detected_video_track_count
          << " video tracks";
  duration_ = params.duration;
}

bool StreamParserExtractor::NewConfigF(
    std::unique_ptr<media::MediaTracks> media_tracks,
    const StreamParser::TextTrackConfigMap& text_tracks) {
  VLOG(5) << __FUNCTION__;
  if (is_seeking_) {
    is_seeking_ = false;
    return true;
  }

  if (tracks_registered_) {
    LOG(WARNING) << "Tracks already registered, rejecting";
    return false;
  }

  for (size_t i = 0; i < media_tracks->tracks().size(); i++) {
    const auto& media_track = media_tracks->tracks().at(i);

    LOG(INFO) << "Track " << i << " type=" << media_track->type()
              << " id=" << media_track->id() << " kind=" << media_track->kind()
              << " label=" << media_track->label()
              << " language=" << media_track->language();

    int track_id = i;

    TrackOutputInterface* out_track = output_->RegisterTrack(track_id);
    auto insert_result = track_map_.insert(std::make_pair(track_id, out_track));

    DCHECK(insert_result.second)
        << "Track '" << media_track->id() << "' registered more than once";

    switch (media_track->type()) {
      case media::MediaTrack::Text:
        // TODO(adewhurst): Support text tracks
        continue;
      case media::MediaTrack::Video: {
        media::VideoDecoderConfig vc =
            media_tracks->getVideoConfig(media_track->id());

        if (!vc.IsValidConfig()) {
          LOG(WARNING) << "Track '" << media_track->id()
                       << "' video config not valid. Ignoring.";
          continue;
        }

        const std::vector<uint8_t>& init_data = vc.extra_data();
        std::unique_ptr<char[]> in_d;
        if (!init_data.empty()) {
          in_d.reset(new char[init_data.size()]);
          std::memcpy(in_d.get(), init_data.data(), init_data.size());
        }

        media::Size video_size = vc.coded_size();
        media::Size natural_size = vc.natural_size();
        std::string codec;
        switch (vc.codec()) {
          case media::VideoCodec::kCodecH264:
            codec = kVideoCodecH264;
            break;
          default:
            LOG(ERROR) << "Unsupported video codec";
            break;
        }
        // TODO(adewhurst): Do a better job finding MIME type? See
        //                  extractors/mp4/AtomParser.java, look for mimeType
        const char* mime_type = util::kVideoMP4;
        // TODO(adewhurst): Get rotation from tkhd atom, if we need to implement
        //                  rotation.
        int32_t rotation_degrees = 0;
        // ExoPlayer uses float for pixel ratio; no need for double
        float pixel_ratio = natural_size.width() / video_size.width();
        // bitrate and input size are left as NO_VALUE in ExoPlayer
        out_track->GiveFormat(MediaFormat::CreateVideoFormat(
            media_track->id(), mime_type, codec, kNoValue, kNoValue,
            duration_.InMicroseconds(), video_size.width(), video_size.height(),
            std::move(in_d), init_data.size(), rotation_degrees, pixel_ratio));
      } break;
      case media::MediaTrack::Audio: {
        media::AudioDecoderConfig ac =
            media_tracks->getAudioConfig(media_track->id());

        if (!ac.IsValidConfig()) {
          LOG(WARNING) << "Track '" << media_track->id()
                       << "' video config not valid. Ignoring.";
          continue;
        }

        const std::vector<uint8_t>& init_data = ac.extra_data();
        std::unique_ptr<char[]> in_d;
        if (!init_data.empty()) {
          in_d.reset(new char[init_data.size()]);
          std::memcpy(in_d.get(), init_data.data(), init_data.size());
        }

        std::string codec;
        switch (ac.codec()) {
          case media::AudioCodec::kCodecAAC:
            codec = kAudioCodecAAC;
            break;
          case media::AudioCodec::kCodecAC3:
            codec = kAudioCodecAC3;
            break;
          case media::AudioCodec::kCodecEAC3:
            codec = kAudioCodecEAC3;
            break;
          default:
            LOG(ERROR) << "Unsupported audio codec";
            break;
        }

        // TODO(adewhurst): Do a better job finding MIME type? See
        //                  extractors/mp4/AtomParser.java, look for mimeType
        const char* mime_type = util::kAudioMP4;
        // TODO(adewhurst): If PCM, set proper PCM type
        int32_t pcm_encoding = kNoValue;

        // bitrate and input size are left as NO_VALUE in ExoPlayer for all code
        // paths I could find.
        out_track->GiveFormat(MediaFormat::CreateAudioFormat(
            media_track->id(), mime_type, codec, kNoValue, kNoValue,
            duration_.InMicroseconds(),
            ChannelLayoutToChannelCount(ac.channel_layout()),
            ac.samples_per_second(), std::move(in_d), init_data.size(),
            media_track->language(), pcm_encoding,
            ChromiumChannelLayoutToNDash(ac.channel_layout()),
            ChromiumSampleFormatToNDash(ac.sample_format())));
      } break;
      default:
        LOG(WARNING) << "Track '" << media_track->id()
                     << "' is unknown type. Ignoring.";
        continue;
    }
  }

  output_->DoneRegisteringTracks();

  tracks_registered_ = true;
  return true;
}

bool StreamParserExtractor::NewBuffersF(
    const StreamParser::BufferQueue& audio,
    const StreamParser::BufferQueue& video,
    const StreamParser::TextBufferQueueMap& text) {
  VLOG(5) << __FUNCTION__;

  if (!tracks_registered_) {
    LOG(WARNING) << "Buffers received without tracks registered. Rejecting.";
    return false;
  }

  DumpBuffers("audio_buffers", audio);
  DumpBuffers("video_buffers", video);

  // TODO(adewhurst): Support text
  if (!text.empty())
    return false;

  return true;
}

void StreamParserExtractor::NewSIDX(
    std::unique_ptr<std::vector<uint32_t>> sizes,
    std::unique_ptr<std::vector<uint64_t>> offsets,
    std::unique_ptr<std::vector<uint64_t>> durations_us,
    std::unique_ptr<std::vector<uint64_t>> times_us) {
  VLOG(5) << __FUNCTION__;

  // TODO(adewhurst): We should rework the ChunkIndex / SeekMapInterface flow.
  //                  This will likely be required to get seek working.
  //
  // Problems:
  // 1. Right now, if this callback runs later, it will probably hit the empty
  //    ContainerMediaChunk::SetSeekMap() rather than do anything useful.
  // 2. DashChunkSource has to cast the SeekMapInterface back to ChunkIndex,
  //    which smells wrong. Perhaps we can just standardize on ChunkIndex (or
  //    add the appropriate methods to SeekMapInterface) since we are only
  //    supporting a subset of the stream types that ExoPlayer supports.
  std::unique_ptr<ChunkIndex> seek_map(
      new ChunkIndex(std::move(sizes), std::move(offsets),
                     std::move(durations_us), std::move(times_us)));
  output_->GiveSeekMap(std::move(seek_map));
}

void StreamParserExtractor::NewMediaSegmentF() {
  VLOG(5) << __FUNCTION__;
}

void StreamParserExtractor::EndMediaSegmentF() {
  VLOG(5) << __FUNCTION__;
  if (reverse_frames_ && !reversed_buffers_.empty()) {
    ProcessBuffers(reversed_buffers_);
    reversed_buffers_.clear();
  }
}

void StreamParserExtractor::EncryptedMediaInitDataF(
    media::EmeInitDataType data_type,
    const std::vector<uint8_t>& init_data) {
  VLOG(5) << __FUNCTION__ << " type=" << (int)data_type
          << " size=" << init_data.size();
  // This is probably not correct, but is probably along the approximate lines
  // of what's required.

  const char* mime_type;
  switch (data_type) {
    case media::EmeInitDataType::CENC:
      mime_type = "video/mp4";
      break;
    default:
      // Non-standard; made up for the purpose of SteamParserExtractor
      mime_type = "application/x-unknown-drm";
      break;
  }

  std::unique_ptr<char[]> init_data_array(new char[init_data.size()]);
  for (size_t i = 0; i < init_data.size(); i++) {
    init_data_array[i] = init_data[i];
  }

  // Launch a license request if we need one for this pssh.
  drm_session_manager_->Request(init_data_array.get(), init_data.size());

  std::unique_ptr<drm::SchemeInitData> scheme_init_data(new drm::SchemeInitData(
      mime_type, std::move(init_data_array), init_data.size()));

  // TODO(adewhurst): Extract UUID(s) and use MappedDrmInitData, if needed
  scoped_refptr<drm::RefCountedDrmInitData> drm_init_data(
      new drm::UniversalDrmInitData(std::move(scheme_init_data)));
  output_->SetDrmInitData(std::move(drm_init_data));
}

void StreamParserExtractor::DumpBuffers(
    const std::string& label,
    const StreamParser::BufferQueue& buffers) {
  VLOG(5) << __FUNCTION__ << ": " << label << " size " << buffers.size();

  if (reverse_frames_) {
    for (const auto& buf : buffers) {
      reversed_buffers_.push_front(buf);
    }
  } else {
    ProcessBuffers(buffers);
  }
}

void StreamParserExtractor::ProcessBuffers(
    const StreamParser::BufferQueue& buffers) {
  for (const auto& buf : buffers) {
    if (buf->end_of_stream()) {
      // TODO(adewhurst): Figure out what to do here
      LOG(INFO) << "end_of_stream";
      return;
    }

    VLOG(1) << "size=" << buf->data_size()
            << ", dur=" << buf->duration().InMilliseconds()
            << ", timestamp=" << buf->GetDecodeTimestamp().InMilliseconds();

    auto track_iter = track_map_.find(buf->track_id());

    if (track_iter == track_map_.end()) {
      LOG(WARNING) << "Couldn't find track ID " << buf->track_id()
                   << " corresponding to buffer of type " << buf->GetTypeName();
      // Skip this buffer
      continue;
    }

    TrackOutputInterface* track = track_iter->second;

    // TODO(adewhurst): prefer something like this:
    // track->WriteSampleData(buf->data(), buf->data_size());
    bool ok = true;
    int32_t remain = buf->data_size();
    int32_t pos = 0;
    while (remain > 0) {
      int64_t num_appended;
      ok = track->WriteSampleDataFixThis(
          reinterpret_cast<const char*>(buf->data()) + pos, remain, true,
          &num_appended);
      if (!ok) {
        break;
      }
      remain -= num_appended;
      pos += num_appended;
    }

    if (!ok) {
      // Not sure what to do here. Maybe try continuing on the next buffer.
      continue;
    }

    int flags = buf->is_key_frame() ? util::kSampleFlagSync : 0;
    const std::string* encryption_key_id = nullptr;
    const std::string* iv = nullptr;
    const media::DecryptConfig* decrypt_config = buf->decrypt_config();
    clear_bytes_.clear();
    encrypted_bytes_.clear();
    if (decrypt_config != nullptr && decrypt_config->is_encrypted()) {
      flags |= util::kSampleFlagEncrypted;
      encryption_key_id = &decrypt_config->key_id();
      iv = &decrypt_config->iv();
      const std::vector<media::SubsampleEntry>& subsamples =
          decrypt_config->subsamples();

      for (int i = 0; i < subsamples.size(); i++) {
        clear_bytes_.push_back(subsamples.at(i).clear_bytes);
        encrypted_bytes_.push_back(subsamples.at(i).cypher_bytes);
      }
      // TODO(rmrossi): When InfoQueue is refactored, consider passing
      // unique_ptr for the clear/encrypted byte counts arrays instead of
      // copying and throwing away what we created.
      track->WriteSampleMetadata(buf->timestamp().InMicroseconds(),
                                 buf->duration().InMicroseconds(), flags,
                                 buf->data_size(), 0, encryption_key_id, iv,
                                 &clear_bytes_, &encrypted_bytes_);
    } else {
      track->WriteSampleMetadata(buf->timestamp().InMicroseconds(),
                                 buf->duration().InMicroseconds(), flags,
                                 buf->data_size(), 0);
    }
  }
}

}  // namespace extractor
}  // namespace ndash
