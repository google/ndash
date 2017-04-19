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

#ifndef NDASH_EXTRACTOR_EXTRACTOR_H_
#define NDASH_EXTRACTOR_EXTRACTOR_H_

#include <cstdint>

#include "upstream/constants.h"

namespace ndash {
namespace extractor {

class ExtractorInputInterface;
class ExtractorOutputInterface;

class ExtractorInterface {
 public:
  ExtractorInterface() {}
  virtual ~ExtractorInterface() {}

  // Returned by Read() if the ExtractorInputInterface passed to the next
  // Read() is required to provide data continuing from the position in the
  // stream reached by the returning call.
  static constexpr int RESULT_CONTINUE = upstream::RESULT_CONTINUE;

  // Returned by Read() if the ExtractorInputInterface passed to the next
  // Read() is required to provide data starting from a specified position in
  // the stream.
  static constexpr int RESULT_SEEK = 1;

  // Returned by Read() if the end of the ExtractorInputInterface was reached.
  // Equal to DashplayerUpstreamConstants::RESULT_END_OF_INPUT.
  static constexpr int RESULT_END_OF_INPUT = upstream::RESULT_END_OF_INPUT;

  // Returned by Read() if there is an I/O error trying to read from the
  // ExtractorInputInterface.
  // Equal to DashplayerUpstreamConstants::RESULT_IO_ERROR.
  static constexpr int RESULT_IO_ERROR = upstream::RESULT_IO_ERROR;

  // Initializes the extractor with an ExtractorOutputInterface.
  // output: An ExtractorOutputInterface to receive extracted data.
  virtual void Init(ExtractorOutputInterface* output) = 0;

  // Returns whether this extractor can extract samples from the
  // ExtractorInputInterface, which must provide data from the start of the
  // stream.
  //
  // If true is returned, the input's reading position may have been modified.
  // Otherwise, only its peek position may have been modified.
  //
  // input: The ExtractorInputInterface from which data should be peeked/read.
  // Returns whether this extractor can read the provided input.
  virtual bool Sniff(ExtractorInputInterface* input) = 0;

  // Extracts data read from a provided ExtractorInputInterface.
  //
  // A single call to this method will block until some progress has been made,
  // but will not block for longer than this. Hence each call will consume only
  // a small amount of input data.
  //
  // In the common case, RESULT_CONTINUE is returned to indicate that the
  // ExtractorInputInterface passed to the next read is required to provide
  // data continuing from the position in the stream reached by the returning
  // call. If the extractor requires data to be provided from a different
  // position, then that position is set in seek_position and RESULT_SEEK is
  // returned. If the extractor reached the end of the data provided by the
  // ExtractorInputInterface, then RESULT_END_OF_INPUT is returned.
  //
  // input: The ExtractorInputInterface from which data should be read.
  // seek_position: If RESULT_SEEK is returned, this holder is updated to hold
  //                the position of the required data.
  // Returns one of the RESULT_ values defined in this class.
  virtual int Read(ExtractorInputInterface* input, int64_t* seek_position) = 0;

  // Notifies the extractor that a seek has occurred.
  //
  // Following a call to this method, the ExtractorInputInterface passed to the
  // next invocation of Read() is required to provide data starting from a
  // random access position in the stream. Valid random access positions are
  // the start of the stream and positions that can be obtained from any
  // SeekMapInterface passed to the ExtractorOutputInterface.
  virtual void Seek() = 0;

  // Releases all kept resources.
  virtual void Release() = 0;
};

}  // namespace extractor
}  // namespace ndash

#endif  // NDASH_EXTRACTOR_EXTRACTOR_H_
