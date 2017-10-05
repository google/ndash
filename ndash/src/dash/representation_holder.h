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

#ifndef NDASH_DASH_REPRESENTATION_HOLDER_H_
#define NDASH_DASH_REPRESENTATION_HOLDER_H_

#include <base/memory/ref_counted.h>
#include <cstdint>
#include <memory>

#include "base/time/time.h"

namespace ndash {

class MediaFormat;

namespace chunk {
class ChunkExtractorWrapper;
}  // namespace chunk

namespace mpd {
class DashSegmentIndexInterface;
class RangedUri;
class Representation;
}  // namespace mpd

namespace dash {

// Internal to DashChunkSource, holds representations with some extra metadata
class RepresentationHolder {
 public:
  RepresentationHolder(
      base::TimeDelta period_start_time,
      base::TimeDelta period_duration,
      const mpd::Representation* representation,
      scoped_refptr<chunk::ChunkExtractorWrapper> extractor_wrapper);
  ~RepresentationHolder();

  const scoped_refptr<chunk::ChunkExtractorWrapper> extractor_wrapper() const {
    return extractor_wrapper_.get();
  }

  scoped_refptr<chunk::ChunkExtractorWrapper> extractor_wrapper() {
    return extractor_wrapper_.get();
  }

  const mpd::Representation* representation() const { return representation_; }

  // This pointer is only valid for the current DashThread task.  It is not
  // safe to store it since DashThread update may cause it to become invalid
  // before the next tasks's execution.
  const mpd::DashSegmentIndexInterface* segment_index() const {
    return segment_index_;
  }

  // This pointer is only valid for the current DashThread task.  It is not
  // safe to store it since DashThread update may cause it to become invalid
  // before the next tasks's execution.
  const MediaFormat* media_format() const { return media_format_.get(); }

  void GiveMediaFormat(std::unique_ptr<const MediaFormat> media_format);
  void GiveSegmentIndex(
      std::unique_ptr<const mpd::DashSegmentIndexInterface> segment_index);

  // Called when the Manifest is updated and the representation pointer needs
  // to be updated.
  // Returns true if successful, false otherwise (BehindLiveWindowException)
  bool UpdateRepresentation(base::TimeDelta new_period_duration,
                            const mpd::Representation* new_representation);

  int32_t GetSegmentNum(base::TimeDelta position) const;
  base::TimeDelta GetSegmentStartTime(int32_t segment_num) const;
  base::TimeDelta GetSegmentEndTime(int32_t segment_num) const;
  int32_t GetFirstSegmentNum() const;
  int32_t GetLastSegmentNum() const;
  bool IsBeforeFirstSegment(int32_t segment_num) const;
  bool IsBeyondLastSegment(int32_t segment_num) const;
  int32_t GetFirstAvailableSegmentNum() const;
  std::unique_ptr<mpd::RangedUri> GetSegmentUri(int segment_num) const;

 private:
  const base::TimeDelta period_start_time_;
  base::TimeDelta period_duration_;
  int32_t segment_num_shift_ = 0;

  const scoped_refptr<chunk::ChunkExtractorWrapper> extractor_wrapper_;

  const mpd::Representation* representation_;

  std::unique_ptr<const mpd::DashSegmentIndexInterface> owned_segment_index_;

  std::unique_ptr<const MediaFormat> media_format_;
  const mpd::DashSegmentIndexInterface* segment_index_;

  RepresentationHolder(const RepresentationHolder& other) = delete;
  RepresentationHolder& operator=(const RepresentationHolder& other) = delete;
};

}  // namespace dash
}  // namespace ndash

#endif  // NDASH_DASH_REPRESENTATION_HOLDER_H_
