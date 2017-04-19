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

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "mp4/audio_decoder_config.h"
#include "mp4/decoder_buffer.h"
#include "mp4/es_descriptor.h"
#include "mp4/media_log.h"
#include "mp4/media_tracks.h"
#include "mp4/mp4_stream_parser.h"
#include "mp4/video_decoder_config.h"

namespace media {

namespace mp4 {

class SegmentParser {
 public:
  SegmentParser()
      : media_log_(new MediaLog()),
        configs_received_(false),
        lower_bound_(
            DecodeTimestamp::FromPresentationTime(base::TimeDelta::Max())) {
    std::set<int> audio_object_types;
    // TODO(jfthibert) Add AC3
    audio_object_types.insert(kISO_14496_3);
    parser_.reset(new MP4StreamParser(audio_object_types, false));
  }

  bool ParseMP4File(const std::string& filename, int append_bytes) {
    InitializeParser();
    scoped_refptr<DecoderBuffer> buffer = ReadTestDataFile(filename);
    // TODO(jfthibert) Verify result
    AppendDataInPieces(buffer->data(), buffer->data_size(), append_bytes);
    return true;
  }

 private:
  scoped_refptr<MediaLog> media_log_;
  std::unique_ptr<MP4StreamParser> parser_;
  bool configs_received_;
  std::unique_ptr<MediaTracks> media_tracks_;
  AudioDecoderConfig audio_decoder_config_;
  VideoDecoderConfig video_decoder_config_;
  DecodeTimestamp lower_bound_;

  bool AppendData(const uint8_t* data, size_t length) {
    return parser_->Parse(data, length);
  }

  bool AppendDataInPieces(const uint8_t* data,
                          size_t length,
                          size_t piece_size) {
    const uint8_t* start = data;
    const uint8_t* end = data + length;
    while (start < end) {
      size_t append_size =
          std::min(piece_size, static_cast<size_t>(end - start));
      if (!AppendData(start, append_size))
        return false;
      start += append_size;
    }
    return true;
  }

  void InitF(const StreamParser::InitParameters& params) {
    LOG(INFO) << "Init: dur=" << params.duration.InMicroseconds()
              << ", autoTimestampOffset="
              << params.auto_update_timestamp_offset;
  }

  bool NewConfigF(std::unique_ptr<MediaTracks> tracks,
                  const StreamParser::TextTrackConfigMap& tc) {
    configs_received_ = true;
    CHECK(tracks.get());
    media_tracks_ = std::move(tracks);
    audio_decoder_config_ = media_tracks_->getFirstAudioConfig();
    video_decoder_config_ = media_tracks_->getFirstVideoConfig();
    LOG(INFO) << "NewConfigF: track count=" << media_tracks_->tracks().size()
              << " audio=" << audio_decoder_config_.IsValidConfig()
              << " video=" << video_decoder_config_.IsValidConfig();
    return true;
  }

  void DumpBuffers(const std::string& label,
                   const StreamParser::BufferQueue& buffers) {
    LOG(INFO) << "DumpBuffers: " << label << " size " << buffers.size();
    for (StreamParser::BufferQueue::const_iterator buf = buffers.begin();
         buf != buffers.end(); buf++) {
      LOG(INFO) << "  n=" << buf - buffers.begin()
                << ", size=" << (*buf)->data_size()
                << ", dur=" << (*buf)->duration().InMilliseconds()
                << ", timestamp=" << (*buf)->timestamp().InMilliseconds();
      const DecryptConfig* decrypt_config = (*buf)->decrypt_config();
      if (decrypt_config->is_encrypted()) {
        const std::vector<SubsampleEntry>& subsamples =
            decrypt_config->subsamples();
        if (!subsamples.empty()) {
          LOG(INFO) << "  subsamples ";
          for (size_t k = 0; k < subsamples.size(); k++) {
            LOG(INFO) << "  " << subsamples[k].clear_bytes << ","
                      << subsamples[k].cypher_bytes;
          }
        }
      }
    }
  }

  bool NewBuffersF(const StreamParser::BufferQueue& audio_buffers,
                   const StreamParser::BufferQueue& video_buffers,
                   const StreamParser::TextBufferQueueMap& text_map) {
    DumpBuffers("audio_buffers", audio_buffers);
    DumpBuffers("video_buffers", video_buffers);

    // TODO(wolenetz/acolwell): Add text track support to more MSE parsers. See
    // http://crbug.com/336926.
    if (!text_map.empty())
      return false;

    // Find the second highest timestamp so that we know what the
    // timestamps on the next set of buffers must be >= than.
    DecodeTimestamp audio = !audio_buffers.empty()
                                ? audio_buffers.back()->GetDecodeTimestamp()
                                : kNoDecodeTimestamp();
    DecodeTimestamp video = !video_buffers.empty()
                                ? video_buffers.back()->GetDecodeTimestamp()
                                : kNoDecodeTimestamp();
    DecodeTimestamp second_highest_timestamp =
        (audio == kNoDecodeTimestamp() ||
         (video != kNoDecodeTimestamp() && audio > video))
            ? video
            : audio;

    DCHECK(second_highest_timestamp != kNoDecodeTimestamp());

    if (lower_bound_ != kNoDecodeTimestamp() &&
        second_highest_timestamp < lower_bound_) {
      return false;
    }

    lower_bound_ = second_highest_timestamp;
    return true;
  }

  void KeyNeededF(EmeInitDataType type, const std::vector<uint8_t>& init_data) {
    LOG(INFO) << "KeyNeededF: " << init_data.size();
    //     EXPECT_EQ(EmeInitDataType::CENC, type);
    //     EXPECT_FALSE(init_data.empty());
  }

  void NewSIDX(std::unique_ptr<std::vector<uint32_t>> sizes,
               std::unique_ptr<std::vector<uint64_t>> offsets,
               std::unique_ptr<std::vector<uint64_t>> durations_us,
               std::unique_ptr<std::vector<uint64_t>> times_us) {
    LOG(INFO) << "NewSIDX";
  }

  void NewSegmentF() {
    LOG(INFO) << "NewSegmentF";
    lower_bound_ = kNoDecodeTimestamp();
  }

  void EndOfSegmentF() {
    LOG(INFO) << "EndOfSegmentF()";
    lower_bound_ =
        DecodeTimestamp::FromPresentationTime(base::TimeDelta::Max());
  }

  void InitializeParser() {
    parser_->Init(
        base::Bind(&SegmentParser::InitF, base::Unretained(this)),
        base::Bind(&SegmentParser::NewConfigF, base::Unretained(this)),
        base::Bind(&SegmentParser::NewBuffersF, base::Unretained(this)), true,
        base::Bind(&SegmentParser::KeyNeededF, base::Unretained(this)),
        base::Bind(&SegmentParser::NewSegmentF, base::Unretained(this)),
        base::Bind(&SegmentParser::EndOfSegmentF, base::Unretained(this)),
        base::Bind(&SegmentParser::NewSIDX, base::Unretained(this)),
        media_log_);
  }

  scoped_refptr<DecoderBuffer> ReadTestDataFile(const std::string& name) {
    base::FilePath file_path(name);

    int64_t tmp = 0;
    CHECK(base::GetFileSize(file_path, &tmp))
        << "Failed to get file size for '" << name << "'";

    int file_size = base::checked_cast<int>(tmp);

    scoped_refptr<DecoderBuffer> buffer(new DecoderBuffer(file_size));
    CHECK_EQ(file_size,
             base::ReadFile(file_path,
                            reinterpret_cast<char*>(buffer->writable_data()),
                            file_size))
        << "Failed to read '" << name << "'";

    return buffer;
  }
};

}  // namespace mp4

}  // namespace media

int main(int argc, char* argv[]) {
  LOG(INFO) << "Starting SegmentParser";
  mp4::SegmentParser dash_player_video;
  mp4::SegmentParser dash_player_audio;
  dash_player_video.ParseMP4File("vid0.encrypted", 32768);
  dash_player_audio.ParseMP4File("aud0.encrypted", 32768);
}
