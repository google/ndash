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

#ifndef NDASH_EXTRACTOR_UNBUFFERED_EXTRACTOR_INPUT_H_
#define NDASH_EXTRACTOR_UNBUFFERED_EXTRACTOR_INPUT_H_

#include <cstdint>
#include <cstdlib>

#include "extractor/extractor_input.h"

namespace ndash {

namespace upstream {
class DataSourceInterface;
}  // namespace upstream

namespace extractor {

// A simplistic implementation of ExtractorInputInterface because we don't
// actually need Peek() and Skip() support for StreamParserExtractor.
// DefaultExtractorInput is more complicated because it uses an extra buffer to
// handle the difference between read position and peek position.
//
// Read() is mostly just a passthru with some record keeping to keep
// GetPosition() working. Other methods that were trivial to support are also
// implemented; the others are not implemented at all (and will trigger
// LOG(FATAL)).
class UnbufferedExtractorInput : public ExtractorInputInterface {
 public:
  UnbufferedExtractorInput(upstream::DataSourceInterface* data_source,
                           int64_t position,
                           int64_t length);
  ~UnbufferedExtractorInput() override;

  ssize_t Read(void* buffer, size_t length) override;

  void ResetPeekPosition() override;
  int64_t GetPeekPosition() const override;
  int64_t GetPosition() const override;
  int64_t GetLength() const override;

  // These are all unimplemented
  bool ReadFully(void* buffer,
                 size_t length,
                 bool* end_of_input = nullptr) override;
  ssize_t Skip(size_t length) override;
  bool SkipFully(size_t length, bool* end_of_input = nullptr) override;
  bool PeekFully(void* buffer,
                 size_t length,
                 bool* end_of_input = nullptr) override;
  bool AdvancePeekPosition(size_t length,
                           bool* end_of_input = nullptr) override;

 private:
  upstream::DataSourceInterface* data_source_;
  int64_t position_;
  int64_t length_;
};

}  // namespace extractor
}  // namespace ndash

#endif  // NDASH_EXTRACTOR_UNBUFFERED_EXTRACTOR_INPUT_H_
