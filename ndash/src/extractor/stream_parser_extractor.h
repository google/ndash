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

#ifndef NDASH_EXTRACTOR_STREAM_PARSER_EXTRACTOR_H_
#define NDASH_EXTRACTOR_STREAM_PARSER_EXTRACTOR_H_

#include <map>
#include <memory>

#include "base/containers/small_map.h"
#include "base/memory/ref_counted.h"
#include "extractor/extractor.h"
#include "mp4/eme_constants.h"
#include "mp4/media_log.h"
#include "mp4/stream_parser.h"
#include "mp4/stream_parser_buffer.h"

namespace media {
class MediaTracks;
}  // namespace media

namespace ndash {

namespace drm {
class DrmSessionManagerInterface;
}  // namespace drm

namespace extractor {

class RollingSampleBuffer;
class TrackOutputInterface;

class StreamParserExtractor : public ExtractorInterface {
 public:
  // If |reverse_frames| is true, frames will be pushed into the sample queue
  // in reverse order.
  StreamParserExtractor(drm::DrmSessionManagerInterface* drm_session_manager,
                        std::unique_ptr<media::StreamParser> parser,
                        scoped_refptr<media::MediaLog> media_log,
                        bool reverse_frames = false);
  ~StreamParserExtractor() override;

  void Init(ExtractorOutputInterface* output) override;
  bool Sniff(ExtractorInputInterface* input) override;
  int Read(ExtractorInputInterface* input, int64_t* seek_position) override;
  void Seek() override;
  void Release() override;

 private:
  using StreamParser = media::StreamParser;
  friend class StreamParserExtractorTest;

  void InitF(const StreamParser::InitParameters& params);
  bool NewConfigF(std::unique_ptr<media::MediaTracks> media_tracks,
                  const StreamParser::TextTrackConfigMap& text_tracks);
  bool NewBuffersF(const StreamParser::BufferQueue& audio,
                   const StreamParser::BufferQueue& video,
                   const StreamParser::TextBufferQueueMap& text);
  void NewMediaSegmentF();
  void EndMediaSegmentF();
  void EncryptedMediaInitDataF(media::EmeInitDataType data_type,
                               const std::vector<uint8_t>& init_data);
  void DumpBuffers(const std::string& label,
                   const StreamParser::BufferQueue& buffers);
  void NewSIDX(std::unique_ptr<std::vector<uint32_t>> sizes,
               std::unique_ptr<std::vector<uint64_t>> offsets,
               std::unique_ptr<std::vector<uint64_t>> durations_us,
               std::unique_ptr<std::vector<uint64_t>> times_us);

  void ProcessBuffers(const StreamParser::BufferQueue& buffers);

  std::unique_ptr<StreamParser> parser_;
  scoped_refptr<media::MediaLog> media_log_;
  base::SmallMap<std::map<StreamParser::TrackId, TrackOutputInterface*>>
      track_map_;

  ExtractorOutputInterface* output_ = nullptr;
  bool tracks_registered_ = false;
  bool is_seeking_ = false;
  base::TimeDelta duration_;
  drm::DrmSessionManagerInterface* drm_session_manager_;

  std::vector<int32_t> clear_bytes_;
  std::vector<int32_t> encrypted_bytes_;

  bool reverse_frames_;
  media::StreamParser::BufferQueue reversed_buffers_;
};

}  // namespace extractor
}  // namespace ndash

#endif  // NDASH_EXTRACTOR_STREAM_PARSER_EXTRACTOR_H_
