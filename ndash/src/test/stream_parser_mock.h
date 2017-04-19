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

#ifndef MEDIA_BASE_STREAM_PARSER_MOCK_H_
#define MEDIA_BASE_STREAM_PARSER_MOCK_H_

#include "base/memory/ref_counted.h"
#include "gmock/gmock.h"
#include "mp4/stream_parser.h"
#include "mp4/text_track_config.h"

namespace media {

class StreamParserMock : public StreamParser {
 public:
  StreamParserMock();
  ~StreamParserMock() override;

  MOCK_METHOD9(
      Init,
      void(const InitCB& init_cb,
           const NewConfigCB& config_cb,
           const NewBuffersCB& new_buffers_cb,
           bool ignore_text_track,
           const EncryptedMediaInitDataCB& encrypted_media_init_data_cb,
           const NewMediaSegmentCB& new_segment_cb,
           const EndMediaSegmentCB& end_of_segment_cb,
           const NewSIDXCB& new_sidx_cb,
           const scoped_refptr<MediaLog>& media_log));
  MOCK_METHOD0(Flush, void());
  MOCK_METHOD2(Parse, bool(const uint8_t* buf, int size));
};

}  // namespace media

#endif  // MEDIA_BASE_STREAM_PARSER_MOCK_H_
