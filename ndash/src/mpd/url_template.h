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

#ifndef NDASH_MPD_URL_TEMPLATE_
#define NDASH_MPD_URL_TEMPLATE_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ndash {

namespace mpd {

// A template from which URLs can be built.
//
// URLs are built according to the substitution rules defined in
// ISO/IEC 23009-1:2014 5.3.9.4.4.
class UrlTemplate {
 public:
  ~UrlTemplate();

  UrlTemplate(const UrlTemplate& other);

  // Compile an instance from the provided template string.
  static std::unique_ptr<UrlTemplate> Compile(const std::string& template_str);

  // Constructs a Uri from the template, substituting in the provided arguments.
  // Arguments whose corresponding identifiers are not present in the template
  // will be ignored.
  std::string BuildUri(const std::string& representation_id,
                       int32_t segment_number,
                       int32_t bandwidth,
                       int32_t time_val) const;

  // Parses template, placing the decomposed components into the provided
  // arrays. If the return value is N, urlPieces will contain (N+1) strings
  // that must be interleaved with N arguments in order to construct a url.
  // The N identifiers that correspond to the required arguments, together
  // with the tags that define their required formatting, are returned in
  // identifiers and identifierFormatTags respectively.
  static int32_t ParseTemplate(
      const std::string& template_str,
      std::vector<std::string>* url_pieces,
      std::vector<int32_t>* identifiers,
      std::vector<std::string>* identifier_format_tags);

  UrlTemplate& operator=(const UrlTemplate& other);

 private:
  enum Templatables {
    REPRESENTATION_ID = 1,
    NUMBER_ID = 2,
    BANDWIDTH_ID = 3,
    TIME_ID = 4
  };

  std::vector<std::string> url_pieces_;
  std::vector<int32_t> identifiers_;
  std::vector<std::string> identifier_format_tags_;
  int32_t identifier_count_;

  // Internal constructor. Use compile(String) to build instances of this class.
  UrlTemplate(const std::vector<std::string>& url_pieces,
              const std::vector<int32_t>& identifiers,
              const std::vector<std::string>& identifier_format_tags,
              const int32_t identifier_count);
};

}  // namespace mpd

}  // namespace ndash

#endif  // NDASH_MPD_URL_TEMPLATE_
