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

#ifndef NDASH_MEDIA_CLOCK_H_
#define NDASH_MEDIA_CLOCK_H_

#include <cstdint>

#include "media_format.h"

namespace ndash {

// TODO(rmrossi): This will likely not be required in the end.  Unless we
// want to expose the decoder's clock through this interface.
class MediaClockInterface {
 public:
  MediaClockInterface(const MediaFormat& other);
  virtual ~MediaClockInterface();

  // Return the current media position in microseconds.
  virtual int64_t GetPositionUs() = 0;
};

}  // namespace ndash

#endif  // NDASH_MEDIA_CLOCK_H_
