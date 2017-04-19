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

#ifndef NDASH_MEDIA_FORMAT_HOLDER_H_
#define NDASH_MEDIA_FORMAT_HOLDER_H_

#include <cstdint>
#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "drm/drm_init_data.h"
#include "drm/scheme_init_data.h"
#include "media_format.h"

namespace ndash {

// Holds a MediaFormat and corresponding drm scheme initialization data.
struct MediaFormatHolder {
  // The format of the media.
  base::WeakPtr<const MediaFormat> format;

  // Initialization data for drm schemes supported by the media. Null if the
  // media is not encrypted.
  scoped_refptr<const drm::RefCountedDrmInitData> drm_init_data;
};

}  // namespace ndash

#endif  // NDASH_MEDIA_FORMAT_H_
