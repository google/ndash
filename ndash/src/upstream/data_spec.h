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

#ifndef NDASH_UPSTREAM_DATA_SPEC_H_
#define NDASH_UPSTREAM_DATA_SPEC_H_

#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "upstream/uri.h"

namespace ndash {
namespace upstream {

// Defines a region of media data.
class DataSpec {
 public:
  // Permits an underlying network stack to request that the server use gzip
  // compression.
  //
  // Should not typically be set if the data being requested is already
  // compressed (e.g. most audio and video requests). May be set when
  // requesting other data.
  //
  // When a DataSource is used to request data with this flag set, and if the
  // DataSource does make a network request, then the value returned from
  // DataSource::Open() will typically be LENGTH_UNBOUNDED. The data read from
  // DataSource::Read() will be the decompressed data.
  //
  enum DataSpecFlags {
    FLAG_ALLOW_GZIP = 1,
  };

  // Identifies the source from which data should be read.
  const Uri uri;
  // Body for a POST request, nullptr otherwise.
  const std::unique_ptr<const std::string> post_body;
  // The absolute position of the data in the full stream.
  const int64_t absolute_stream_position;
  // The position of the data when read from uri_
  // Always equal to absolute_stream_position_ unless the uri_ defines the
  // location of a subset of the underyling data.
  const int64_t position;
  // The length of the data. Greater than zero, or equal to LENGTH_UNBOUNDED
  const int64_t length;
  // A key that uniquely identifies the original stream. Used for cache
  // indexing. May be null if the DataSpec is not intended to be used in
  // conjunction with a cache.
  const std::unique_ptr<const std::string> key;
  // Request flags. Currently FLAG_ALLOW_GZIP is the only supported flag.
  const int flags;

  // Construct a DataSpec for the given uri and with key_ set to nullptr.
  explicit DataSpec(const Uri& uri);

  // Construct a DataSpec for the given uri and with key_ set to nullptr, and
  // allow setting flags.
  DataSpec(const Uri& uri, int flags);

  // Construct a DataSpec where position_ equals absolute_stream_position_.
  DataSpec(const Uri& uri,
           int64_t absolute_stream_position,
           int64_t length,
           const std::string* key);

  // Construct a DataSpec where position_ equals absolute_stream_position_, and
  // allow setting flags.
  DataSpec(const Uri& uri,
           int64_t absolute_stream_position,
           int64_t length,
           const std::string* key,
           int flags);

  // Construct a DataSpec where position_ may differ from
  // absoluteStreamPosition_.
  DataSpec(const Uri& uri,
           int64_t absolute_stream_position,
           int64_t position,
           int64_t length,
           const std::string* key,
           int flags);

  // Construct a DataSpec with POST data.
  DataSpec(const Uri& uri,
           const std::string* post_body,
           int64_t absolute_stream_position,
           int64_t position,
           int64_t length,
           const std::string* key,
           int flags);

  // Copy & move constructors. There is no move constructor or assignment
  // operator because the class is immutable. This should perhaps be revisited
  // later. Makes a deep copy.
  DataSpec(const DataSpec& other);

  ~DataSpec();

  std::string DebugString() const;

  static DataSpec GetRemainder(const DataSpec& data_spec, int64_t bytes_loaded);
};

}  // namespace upstream
}  // namespace ndash

#endif  // NDASH_UPSTREAM_DATA_SPEC_H_
