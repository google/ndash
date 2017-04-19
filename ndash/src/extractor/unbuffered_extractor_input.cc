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

#include "extractor/unbuffered_extractor_input.h"

#include <cstdint>
#include <cstdlib>

#include "base/logging.h"
#include "upstream/data_source.h"

namespace ndash {
namespace extractor {

UnbufferedExtractorInput::UnbufferedExtractorInput(
    upstream::DataSourceInterface* data_source,
    int64_t position,
    int64_t length)
    : data_source_(data_source), position_(position), length_(length) {}

UnbufferedExtractorInput::~UnbufferedExtractorInput() {}

ssize_t UnbufferedExtractorInput::Read(void* buffer, size_t length) {
  ssize_t result = data_source_->Read(buffer, length);

  if (result > 0) {
    position_ += result;
  }

  return result;
}

bool UnbufferedExtractorInput::ReadFully(void* buffer,
                                         size_t length,
                                         bool* end_of_input) {
  LOG(FATAL) << "Unimplemented " << __FUNCTION__;
  return false;
}

ssize_t UnbufferedExtractorInput::Skip(size_t length) {
  LOG(FATAL) << "Unimplemented " << __FUNCTION__;
  return 0;
}

bool UnbufferedExtractorInput::SkipFully(size_t length, bool* end_of_input) {
  LOG(FATAL) << "Unimplemented " << __FUNCTION__;
  return false;
}

bool UnbufferedExtractorInput::PeekFully(void* buffer,
                                         size_t length,
                                         bool* end_of_input) {
  LOG(FATAL) << "Unimplemented " << __FUNCTION__;
  return false;
}

bool UnbufferedExtractorInput::AdvancePeekPosition(size_t length,
                                                   bool* end_of_input) {
  LOG(FATAL) << "Unimplemented " << __FUNCTION__;
  return false;
}

void UnbufferedExtractorInput::ResetPeekPosition() {
  // Do nothing! Peek position is always the read position.
}
int64_t UnbufferedExtractorInput::GetPeekPosition() const {
  return position_;
}
int64_t UnbufferedExtractorInput::GetPosition() const {
  return position_;
}
int64_t UnbufferedExtractorInput::GetLength() const {
  return length_;
}

}  // namespace extractor
}  // namespace ndash
