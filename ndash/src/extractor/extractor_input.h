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

#ifndef NDASH_EXTRACTOR_EXTRACTOR_INPUT_H_
#define NDASH_EXTRACTOR_EXTRACTOR_INPUT_H_

#include <cstdint>
#include <cstdlib>

namespace ndash {
namespace extractor {

// Provides data to be consumed by an Extractor.
class ExtractorInputInterface {
 public:
  ExtractorInputInterface() {}
  virtual ~ExtractorInputInterface() {}

  // Reads up to 'length' bytes from the input and resets the peek position.
  // This method blocks until at least one byte of data can be read, the end of
  // the input is detected, or an error occurs.
  //
  // buffer: A target buffer into which data should be written.
  // length: The maximum number of bytes to read from the input.
  // Returns the number of bytes read, or
  //   - RESULT_END_OF_INPUT if the input has ended
  //   - RESULT_IO_ERROR if there is an error
  virtual ssize_t Read(void* target, size_t length) = 0;

  // Like Read(), but reads the requested 'length' in full.
  // If the end of the input is found having read no data, then behavior is
  // dependent on 'end_of_input'. If end_of_input == null then false is
  // returned (treated as an error). Otherwise, *end_of_input is set to true if
  // the end of the input is found, or false if 'length' bytes were read.
  //
  // Encountering the end of input having partially satisfied the read is
  // always considered an error, and will result in *end_of_input being set to
  // true (if not null) and false will be returned.
  //
  // buffer: A target buffer into which data should be written.
  // length: The number of bytes to read from the input.
  // end_of_input: Can be null. If null, then encountering the end of input is
  //               always considered an error. If not null, end_of_input is an
  //               output parameter signifying if the end of input has been
  //               detected. Depending on where the end of input has been
  //               detected, it may be an error or not.  end_of_input will
  //               always be set to false if no end of input has been detected.
  // Returns true if the read was successful or if the end of the input was
  // encountered having read no data (and end_of_input != null). Returns false
  // upon error.
  virtual bool ReadFully(void* buffer,
                         size_t length,
                         bool* end_of_input = nullptr) = 0;

  // Like Read(), except the data is skipped instead of read.
  //
  // length: The maximum number of bytes to skip from the input.
  // Returns the number of bytes skipped, or
  //   - RESULT_END_OF_INPUT if the input has ended
  //   - RESULT_IO_ERROR if there is an error
  virtual ssize_t Skip(size_t length) = 0;

  // Like ReadFully(), except the data is skipped instead of read.
  //
  // length: The number of bytes to skip from the input.
  // end_of_input: Can be null. If null, then encountering the end of input is
  //               always considered an error. If not null, end_of_input is an
  //               output parameter signifying if the end of input has been
  //               detected. Depending on where the end of input has been
  //               detected, it may be an error or not.  end_of_input will
  //               always be set to false if no end of input has been detected.
  // Returns true if the skip was successful or if the end of the input was
  // encountered having skipped no data (and end_of_input != null). Returns
  // false upon error.
  virtual bool SkipFully(size_t length, bool* end_of_input = nullptr) = 0;

  // Peeks 'length' bytes from the peek position, writing them into 'target'.
  // The current read position is left unchanged.
  //
  // If the end of the input is found having peeked no data, then behavior is
  // dependent on 'end_of_input'. If end_of_input == null then false is
  // returned (treated as an error). Otherwise, *end_of_input is set to true if
  // the end of the input is found, or false if 'length' bytes were read.
  //
  // Calling ResetPeekPosition() resets the peek position to equal the current
  // read position, so the caller can peek the same data again. Reading and
  // skipping also reset the peek position.
  //
  // buffer: A target buffer into which data should be written.
  // length: The number of bytes to peek from the input.
  // end_of_input: Can be null. If null, then encountering the end of input is
  //               always considered an error. If not null, end_of_input is an
  //               output parameter signifying if the end of input has been
  //               detected. Depending on where the end of input has been
  //               detected, it may be an error or not.  end_of_input will
  //               always be set to false if no end of input has been detected.
  // Returns true if the peek was successful or if the end of the input was
  // encountered having peeked no data (and end_of_input != null). Returns
  // false upon error.
  virtual bool PeekFully(void* buffer,
                         size_t length,
                         bool* end_of_input = nullptr) = 0;

  // Advances the peek position by 'length' bytes. Behaves like SkipFully() but
  // operates on the peek position rather than the read position.
  //
  // length: The number of bytes to (not) peek from the input.
  // end_of_input: Can be null. If null, then encountering the end of input is
  //               always considered an error. If not null, end_of_input is an
  //               output parameter signifying if the end of input has been
  //               detected. Depending on where the end of input has been
  //               detected, it may be an error or not.  end_of_input will
  //               always be set to false if no end of input has been detected.
  // Returns true if the skip was successful or if the end of the input was
  // encountered having skipped no data (and end_of_input != null). Returns
  // false upon error.
  virtual bool AdvancePeekPosition(size_t length,
                                   bool* end_of_input = nullptr) = 0;

  // Resets the peek position to equal the current read position.
  virtual void ResetPeekPosition() = 0;

  // Returns the current peek position (byte offset) in the stream.
  virtual int64_t GetPeekPosition() const = 0;

  // Returns the current read position (byte offset) in the stream.
  virtual int64_t GetPosition() const = 0;

  // Returns the length of the source stream, or LENGTH_UNBOUNDED if it is
  // unknown.
  virtual int64_t GetLength() const = 0;
};

}  // namespace extractor
}  // namespace ndash

#endif  // NDASH_EXTRACTOR_EXTRACTOR_INPUT_H_
