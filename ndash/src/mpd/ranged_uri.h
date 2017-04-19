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

#ifndef NDASH_MPD_RANGED_URI_
#define NDASH_MPD_RANGED_URI_

#include <cstdint>
#include <memory>
#include <string>

namespace ndash {

namespace mpd {

// Defines a range of data located at a Uri.  DASH manifests may specify
// 'indexRange' or 'range' attributes (in the form '[0-9]+-[0-9]+) that
// indicate the data should be fetched from a common url with extra range
// headers (or parameters) added to the request.
class RangedUri {
 public:
  // Constructs a RangedUri. The string pointed to by base_uri is held by this
  // instance and must remain valid for the lifetime of this RangedUri.
  RangedUri(const std::string* base_uri,
            const std::string& reference_uri,
            int64_t start,
            int32_t length);
  ~RangedUri();

  // Returns the uri represented by the instance as a string.
  std::string GetUriString() const;

  // Returns the (zero based) index of the first byte of the range.
  int64_t GetStart() const;

  // Returns the length of the range, or -1 to indicate the range is unbounded.
  int32_t GetLength() const;

  // Attempts to merge this RangedUri with another.
  // A merge is successful if both instances define the same Uri, and if one
  // starts the byte after the other ends, forming a contiguous region with no
  // overlap.
  // If other is null then the merge is considered unsuccessful, and null is
  // returned.
  std::unique_ptr<RangedUri> AttemptMerge(const RangedUri* other) const;

  bool operator==(const RangedUri& other) const;
  RangedUri& operator=(const RangedUri& other);

 private:
  // The URI is stored internally in two parts: reference URI and a base URI
  // to use when resolving it. This helps optimize memory usage in the same way
  // that DASH manifests allow many URLs to be expressed concisely in the form
  // of a single BaseURL and many relative paths. Note that this optimization
  // relies on the same object being passed as the base URI to many instances
  // of this class.
  // Subclasses of SegmentBase own the storage space for base_uri.
  const std::string* base_uri_;
  std::string reference_uri_;

  // The (zero based) index of the first byte of the range.
  int64_t start_;

  // The length of the range, or -1 to indicate that the range is unbounded.
  int32_t length_;
};

}  // namespace mpd

}  // namespace ndash

#endif  // NDASH_MPD_RANGED_URI_
