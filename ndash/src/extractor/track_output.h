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

#ifndef NDASH_EXTRACTOR_TRACK_OUTPUT_H_
#define NDASH_EXTRACTOR_TRACK_OUTPUT_H_

#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

namespace ndash {

class MediaFormat;

namespace extractor {

class ExtractorInputInterface;

// Receives stream level data extracted by an Extractor
class TrackOutputInterface {
 public:
  TrackOutputInterface() {}
  virtual ~TrackOutputInterface() {}

  // Invoked when the MediaFormat of the track has been extracted from the
  // stream.
  // format: The extracted MediaFormat.
  virtual void GiveFormat(std::unique_ptr<const MediaFormat> format) = 0;

  // Invoked to write sample data to the output.
  // input: An ExtractorInputInterface from which to read the sample data.
  // length: The maximum length to read from the input.
  // allow_end_of_input: True if encountering the end of the input having read
  //                     no data is allowed, and should result in a return
  //                     value of true with bytes_appended set to
  //                     RESULT_END_OF_INPUT.  False if it should be considered
  //                     an error, causing false to be returned.
  // bytes_appended: If not null, modified to return the number of bytes
  //                 appended (or possibly RESULT_END_OF_INPUT)
  // Returns true on success, false on error (or if canceled)
  virtual bool WriteSampleData(ExtractorInputInterface* input,
                               size_t max_length,
                               bool allow_end_of_input,
                               int64_t* bytes_appended) = 0;

  // Invoked to write sample data to the output.
  // data: A buffer from which to read the sample data.
  // length: The number of bytes to read.
  virtual void WriteSampleData(const void* data, size_t length) = 0;
  virtual bool WriteSampleDataFixThis(const void* src,
                                      size_t length,
                                      bool allow_end_of_input,
                                      int64_t* num_bytes_written) = 0;

  // Invoked when metadata associated with a sample has been extracted from the
  // stream.
  // The corresponding sample data will have already been passed to the output
  // via calls to WriteSampleData()
  // time_us: The media timestamp associated with the sample, in microseconds.
  // duration_us: The duration of the sample, in microseconds
  // flags: Flags associated with the sample. See SampleHolder flags.
  // size: The size of the sample data, in bytes.
  // offset: The number of bytes that have been passed to WriteSampleData()
  //         since the last byte belonging to the sample whose metadata is
  //         being passed.
  // encryption_key_id: The encryption key associated with the sample. May be
  //         null.
  // iv : The initialization vector if the sample is encrypted. May be null.
  // num_bytes_clear: The clear byte counts if the sample is encrypted, or null.
  // num_bytes_enc: The encrypted byte counts, or null.
  //
  // Note: Encrypted data may contain clear (unencrypted) and encrypted regions.
  // The num_bytes_clear and num_bytes_enc arrays describe the number of bytes
  // of each type in each back-to-back region of the data starting at pos 0.
  // TODO(rmrossi): Use a vector of structs instead of two vectors like
  // MP4 parser does.  Then each i'th element describes one region more clearly.
  virtual void WriteSampleMetadata(
      int64_t time_us,
      int64_t duration_us,
      int32_t flags,
      size_t size,
      size_t offset,
      const std::string* encryption_key_id = nullptr,
      const std::string* iv = nullptr,
      std::vector<int32_t>* num_bytes_clear = nullptr,
      std::vector<int32_t>* num_bytes_enc = nullptr) = 0;
};

}  // namespace extractor
}  // namespace ndash

#endif  // NDASH_EXTRACTOR_TRACK_OUTPUT_H_
