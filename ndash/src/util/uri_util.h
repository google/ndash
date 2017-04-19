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

#ifndef NDASH_UTIL_URI_UTIL_H_
#define NDASH_UTIL_URI_UTIL_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string>

#include "base/strings/string_piece.h"

namespace ndash {

namespace util {

class UriUtil {
 public:
  // Performs relative resolution of a referenceUri with respect to a baseUri.
  // The resolution is performed as specified by RFC-3986.
  static std::string Resolve(const std::string& base_uri,
                             const std::string& reference_uri);

  // Get the value of a query parameter by name.
  static base::StringPiece GetQueryParam(const std::string& uri_string,
                                         const std::string& param);

  // Remove a query parameter by name from the uri.
  static std::string RemoveQueryParam(const std::string& uri_string,
                                      const std::string& param);

  // Unescape a query parameter
  static std::string DecodeQueryComponent(std::string url);

 private:
  enum PartIndices {
    // An index into an array returned by getUriIndices.
    // The value at this position in the array is the index of the ':' after
    // the scheme. Equals -1 if the URI is a relative reference (no scheme).
    // The hier-part starts at (schemeColon + 1), including when the URI has no
    // scheme.
    SCHEME_COLON = 0,
    // An index into an array returned by getUriIndices.
    // The value at this position in the array is the index of the path part.
    // Equals (schemeColon + 1) if no authority part, (schemeColon + 3) if the
    // authority part consists of just "//", and (query) if no path part. The
    // characters starting at this index can be "//" only if the authority part
    // is non-empty (in this case the double-slash means the first segment is
    // empty).
    PATH = 1,
    // An index into an array returned by getUriIndices.
    // The value at this position in the array is the index of the query part,
    // including the '?' before the query. Equals fragment if no query part,
    // and (fragment - 1) if the query part is a single '?' with no data.
    QUERY = 2,
    // An index into an array returned by getUriIndices.
    // The value at this position in the array is the index of the fragment
    // part, including the '#' before the fragment. Equal to the length of the
    // URI if no fragment part, and (length - 1) if the fragment part is a
    // single '#' with no data.
    FRAGMENT = 3,
    // The length of arrays returned by getUriIndices.
    INDEX_COUNT = 4
  };

  UriUtil() = delete;

  // Removes dot segments from the path of a URI.
  static std::string RemoveDotSegments(std::string uri, int offset, int limit);

  // Calculates indices of the constituent components of a URI.
  static std::unique_ptr<int[]> GetUriIndices(const std::string& uri_string);

  // Helper function to separate the query part of a URI from the non-query
  // parts.
  // uri_string: the input URI string
  // query_piece: the destination string piece to hold the query part.
  // scheme_and_path_piece: optional destination string piece to hold the
  //                        scheme and path part.
  // fragment_piece: optional destination string piece to hold the scheme and
  //                 path part.
  static void FindPieces(const std::string& uri_string,
                         base::StringPiece* query_piece,
                         base::StringPiece* scheme_and_path_piece = nullptr,
                         base::StringPiece* fragment_piece = nullptr);
};

}  // namespace util

}  // namespace ndash

#endif  // NDASH_UTIL_URI_UTIL_H_
