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

#include "util/uri_util.h"

#include <curl/curl.h>

#include <algorithm>
#include <map>
#include <string>

namespace ndash {

namespace util {

std::string UriUtil::Resolve(const std::string& base_uri,
                             const std::string& reference_uri) {
  std::string uri;

  std::unique_ptr<int[]> ref_indices = GetUriIndices(reference_uri);
  if (ref_indices[SCHEME_COLON] != -1) {
    // The reference is absolute. The target Uri is the reference.
    uri.append(reference_uri);
    RemoveDotSegments(uri, ref_indices[PATH], ref_indices[QUERY]);
    return uri;
  }

  std::unique_ptr<int[]> base_indices = GetUriIndices(base_uri);
  if (ref_indices[FRAGMENT] == 0) {
    // The reference is empty or contains just the fragment part, then the
    // target Uri is the concatenation of the base Uri without its fragment,
    // and the reference.
    std::string no_frag = base_uri.substr(0, base_indices[FRAGMENT]);
    uri.append(no_frag);
    uri.append(reference_uri);
    return uri;
  }

  if (ref_indices[QUERY] == 0) {
    // The reference starts with the query part. The target is the base up to
    // (but excluding) the query, plus the reference.
    std::string no_query = base_uri.substr(0, base_indices[QUERY]);
    uri.append(no_query);
    uri.append(reference_uri);
    return uri;
  }

  if (ref_indices[PATH] != 0) {
    // The reference has authority. The target is the base scheme plus the
    // reference.
    int base_limit = base_indices[SCHEME_COLON] + 1;
    std::string base = base_uri.substr(0, base_limit);
    uri.append(base);
    uri.append(reference_uri);
    return RemoveDotSegments(uri, base_limit + ref_indices[PATH],
                             base_limit + ref_indices[QUERY]);
  }

  if (ref_indices[PATH] != ref_indices[QUERY] &&
      reference_uri[ref_indices[PATH]] == '/') {
    // The reference path is rooted. The target is the base scheme and
    // authority (if any), plus the reference.
    std::string base = base_uri.substr(0, base_indices[PATH]);
    uri.append(base);
    uri.append(reference_uri);
    return RemoveDotSegments(uri, base_indices[PATH],
                             base_indices[PATH] + ref_indices[QUERY]);
  }

  // The target Uri is the concatenation of the base Uri up to (but excluding)
  // the last segment, and the reference. This can be split into 2 cases:
  if (base_indices[SCHEME_COLON] + 2 < base_indices[PATH] &&
      base_indices[PATH] == base_indices[QUERY]) {
    // Case 1: The base hier-part is just the authority, with an empty path. An
    // additional '/' is needed after the authority, before appending the
    // reference.
    std::string base = base_uri.substr(0, base_indices[PATH]);
    uri.append(base).append("/").append(reference_uri);
    return RemoveDotSegments(uri, base_indices[PATH],
                             base_indices[PATH] + ref_indices[QUERY] + 1);
  } else {
    // Case 2: Otherwise, find the last '/' in the base hier-part and append
    // the reference after it. If base hier-part has no '/', it could only mean
    // that it is completely empty or contains only one segment, in which case
    // the whole hier-part is excluded and the reference is appended right
    // after the base scheme colon without an added '/'.
    std::string query = base_uri.substr(0, base_indices[QUERY]);
    int lastSlashIndex = query.find_last_of('/');
    int baseLimit =
        lastSlashIndex == -1 ? base_indices[PATH] : lastSlashIndex + 1;
    std::string base = base_uri.substr(0, baseLimit);
    uri.append(base);
    uri.append(reference_uri);
    return RemoveDotSegments(uri, base_indices[PATH],
                             baseLimit + ref_indices[QUERY]);
  }
}

std::string UriUtil::RemoveDotSegments(std::string uri, int offset, int limit) {
  if (offset >= limit) {
    // Nothing to do.
    return uri;
  }
  if (uri[offset] == '/') {
    // If the path starts with a /, always retain it.
    offset++;
  }
  // The first character of the current path segment.
  int segment_start = offset;
  int i = offset;
  while (i <= limit) {
    int next_segment_start = -1;
    if (i == limit) {
      next_segment_start = i;
    } else if (uri[i] == '/') {
      next_segment_start = i + 1;
    } else {
      i++;
      continue;
    }
    // We've encountered the end of a segment or the end of the path. If the
    // final segment was "." or "..", remove the appropriate segments of the
    // path.
    if (i == segment_start + 1 && uri[segment_start] == '.') {
      // Given "abc/def/./ghi", remove "./" to get "abc/def/ghi".
      uri.erase(segment_start, next_segment_start - segment_start);
      limit -= next_segment_start - segment_start;
      i = segment_start;
    } else if (i == segment_start + 2 && uri[segment_start] == '.' &&
               uri[segment_start + 1] == '.') {
      // Given "abc/def/../ghi", remove "def/../" to get "abc/ghi".
      std::string prev = uri.substr(0, segment_start - 2);
      int prev_segment_start = prev.find_last_of("/") + 1;
      int removeFrom =
          prev_segment_start > offset ? prev_segment_start : offset;
      uri.erase(removeFrom, next_segment_start - removeFrom);
      limit -= next_segment_start - removeFrom;
      segment_start = prev_segment_start;
      i = prev_segment_start;
    } else {
      i++;
      segment_start = i;
    }
  }
  return uri;
}

std::unique_ptr<int[]> UriUtil::GetUriIndices(const std::string& uri_string) {
  std::unique_ptr<int[]> indices(new int[INDEX_COUNT]);

  indices[SCHEME_COLON] = 0;
  indices[PATH] = 0;
  indices[QUERY] = 0;
  indices[FRAGMENT] = 0;

  if (uri_string.length() == 0) {
    indices[SCHEME_COLON] = -1;
    return indices;
  }

  // Determine outer structure from right to left.
  // Uri = scheme ":" hier-part [ "?" query ] [ "#" fragment ]
  int length = uri_string.length();
  int fragment_index = uri_string.find('#');
  if (fragment_index == -1) {
    fragment_index = length;
  }
  int query_index = uri_string.find('?');
  if (query_index == -1 || query_index > fragment_index) {
    // '#' before '?': '?' is within the fragment.
    query_index = fragment_index;
  }
  // Slashes are allowed only in hier-part so any colon after the first slash
  // is part of the
  // hier-part, not the scheme colon separator.
  int scheme_index_limit = uri_string.find('/');
  if (scheme_index_limit == -1 || scheme_index_limit > query_index) {
    scheme_index_limit = query_index;
  }
  int scheme_index = uri_string.find(':');
  if (scheme_index > scheme_index_limit) {
    // '/' before ':'
    scheme_index = -1;
  }

  // Determine hier-part structure: hier-part = "//" authority path / path
  // This block can also cope with schemeIndex == -1.
  bool has_authority = scheme_index + 2 < query_index &&
                       uri_string[scheme_index + 1] == '/' &&
                       uri_string[scheme_index + 2] == '/';
  int path_index;
  if (has_authority) {
    // find first '/' after "://"
    path_index = uri_string.find("/", scheme_index + 3);
    if (path_index == -1 || path_index > query_index) {
      path_index = query_index;
    }
  } else {
    path_index = scheme_index + 1;
  }

  indices[SCHEME_COLON] = scheme_index;
  indices[PATH] = path_index;
  indices[QUERY] = query_index;
  indices[FRAGMENT] = fragment_index;
  return indices;
}

void UriUtil::FindPieces(const std::string& uri_string,
                         base::StringPiece* query_piece,
                         base::StringPiece* scheme_and_path_piece,
                         base::StringPiece* fragment_piece) {
  std::unique_ptr<int[]> indices = GetUriIndices(uri_string);

  int query_start = indices[QUERY];
  int query_end = indices[FRAGMENT];
  if (query_start == 0) {
    *query_piece = "";
    return;
  }

  query_start++;

  DCHECK(query_piece != nullptr);
  *query_piece = base::StringPiece(uri_string)
                     .substr(query_start, query_end - query_start);

  if (scheme_and_path_piece != nullptr) {
    *scheme_and_path_piece =
        base::StringPiece(uri_string).substr(0, query_start);
  }

  if (fragment_piece != nullptr) {
    *fragment_piece = base::StringPiece(uri_string).substr(query_end);
  }
}

base::StringPiece UriUtil::GetQueryParam(const std::string& uri_string,
                                         const std::string& param_name) {
  base::StringPiece query;
  UriUtil::FindPieces(uri_string, &query);

  int start = 0;
  int end;
  do {
    end = query.find("&", start);
    base::StringPiece param = query.substr(start, end - start);

    int eq = param.find("=");
    if (eq != base::StringPiece::npos) {
      base::StringPiece name = param.substr(0, eq);
      base::StringPiece value = param.substr(eq + 1);
      if (name == param_name) {
        return value;
      }
    }
    start = end + 1;
  } while (end != base::StringPiece::npos);

  return "";
}

// Remove a query parameter by name from the uri.
std::string UriUtil::RemoveQueryParam(const std::string& uri_string,
                                      const std::string& param_name) {
  base::StringPiece query;
  base::StringPiece scheme_and_path;
  base::StringPiece fragment;
  UriUtil::FindPieces(uri_string, &query, &scheme_and_path, &fragment);

  std::string out;
  out.append(scheme_and_path.as_string());

  int start = 0;
  int end;
  bool need_ampersand = false;
  do {
    end = query.find("&", start);
    base::StringPiece param;
    if (end != base::StringPiece::npos) {
      param = query.substr(start, end - start);
    } else {
      param = query.substr(start);
    }

    int eq = param.find("=");
    base::StringPiece name = param.substr(0, eq);
    if (name != param_name) {
      if (need_ampersand) {
        out.append("&");
      }
      need_ampersand = true;
      out.append(param.as_string());
    }
    start = end + 1;
  } while (end != base::StringPiece::npos);

  out.append(fragment.as_string());
  return out;
}

std::string UriUtil::DecodeQueryComponent(std::string url) {
  std::string decoded;
  std::unique_ptr<CURL, void (*)(CURL*)> curl(curl_easy_init(),
                                              curl_easy_cleanup);

  CHECK(curl.get() != nullptr);

  std::replace(url.begin(), url.end(), '+', ' ');

  int outlen;
  char* output =
      curl_easy_unescape(curl.get(), url.c_str(), url.length(), &outlen);
  if (output) {
    decoded = std::string(output, outlen);
    curl_free(output);
  }
  return decoded;
}

}  // namespace util

}  // namespace ndash
