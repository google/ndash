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

#ifndef NDASH_MPD_SINGLE_SEGMENT_BASE_H_
#define NDASH_MPD_SINGLE_SEGMENT_BASE_H_

#include <cstdint>
#include <memory>
#include <string>

#include "mpd/segment_base.h"

namespace ndash {

namespace mpd {

// A SegmentBase that defines a single segment.
class SingleSegmentBase : public SegmentBase {
 public:
  // Construct a SingleSegmentBase.
  // The presentation time offset in seconds is the division of
  // 'presentation_time_offset' and 'timescale'.
  // ('timescale' is in units per second)
  SingleSegmentBase(std::unique_ptr<RangedUri> initialization,
                    int64_t timescale,
                    int64_t presentation_time_offset,
                    std::unique_ptr<std::string> uri,
                    int64_t index_start,
                    int64_t index_length);

  // Construct a SingleSegmentBase from just a uri.
  SingleSegmentBase(std::unique_ptr<std::string> uri);
  ~SingleSegmentBase() override;

  std::unique_ptr<RangedUri> GetIndex() const;

  // Returned reference is valid only for the lifetime of this
  // SingleSegmentBase
  std::string* GetUri() const { return base_url_.get(); }

  bool IsSingleSegment() const override;

  int64_t GetIndexStart() const { return index_start_; }
  int64_t GetIndexLength() const { return index_length_; }

 private:
  int64_t index_start_;
  int64_t index_length_;
};

}  // namespace mpd

}  // namespace ndash

#endif  // NDASH_MPD_SINGLE_SEGMENT_BASE_H_
