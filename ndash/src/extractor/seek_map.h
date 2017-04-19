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

#ifndef NDASH_EXTRACTOR_SEEK_MAP_H_
#define NDASH_EXTRACTOR_SEEK_MAP_H_

#include <cstdint>
#include <memory>

namespace ndash {
namespace extractor {

class SeekMapInterface {
 public:
  SeekMapInterface() {}
  virtual ~SeekMapInterface() {}

  // Whether or not the seeking is supported.
  // If seeking is not supported then the only valid seek position is the start
  // of the file, and so GetPosition() will return 0 for all input values.
  // Returns true if seeking is supported. False otherwise.
  virtual bool IsSeekable() const = 0;

  // Maps a seek position in microseconds to a corresponding position (byte
  // offset) in the stream from which data can be provided to the extractor.
  //
  // time_us: A seek position in microseconds.
  // Returns the corresponding position (byte offset) in the stream from which
  // data can be provided to the extractor, or 0 if IsSeekable() returns false.
  virtual int64_t GetPosition(int64_t time_us) const = 0;
};

class Unseekable : public SeekMapInterface {
 public:
  Unseekable();
  ~Unseekable() override;
  bool IsSeekable() const override;
  int64_t GetPosition(int64_t time_us) const override;
};

}  // namespace extractor
}  // namespace ndash

#endif  // NDASH_EXTRACTOR_SEEK_MAP_H_
