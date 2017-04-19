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

#include "url_template.h"

#include <cstdio>

#include "base/logging.h"

namespace {

constexpr int kUrlBuilderBufSize = 32;
constexpr char kRepresentation[] = "RepresentationID";
constexpr char kNumber[] = "Number";
constexpr char kBandwidth[] = "Bandwidth";
constexpr char kTime[] = "Time";
constexpr char kEscapedDollar[] = "$$";
constexpr char kDefaultFormatTag[] = "%01d";
}  // namespace

namespace ndash {

namespace mpd {

std::unique_ptr<UrlTemplate> UrlTemplate::Compile(
    const std::string& template_str) {
  std::vector<std::string> url_pieces;
  std::vector<int32_t> identifiers;
  std::vector<std::string> identifier_format_tags;
  int identifier_count = ParseTemplate(template_str, &url_pieces, &identifiers,
                                       &identifier_format_tags);
  return std::unique_ptr<UrlTemplate>(new UrlTemplate(
      url_pieces, identifiers, identifier_format_tags, identifier_count));
}

UrlTemplate::UrlTemplate(const std::vector<std::string>& url_pieces,
                         const std::vector<int32_t>& identifiers,
                         const std::vector<std::string>& identifier_format_tags,
                         const int32_t identifier_count)
    : url_pieces_(url_pieces),
      identifiers_(identifiers),
      identifier_format_tags_(identifier_format_tags),
      identifier_count_(identifier_count) {
  DCHECK(identifier_count >= 0);
}

UrlTemplate::~UrlTemplate() {}

UrlTemplate::UrlTemplate(const UrlTemplate& other)
    : url_pieces_(other.url_pieces_),
      identifiers_(other.identifiers_),
      identifier_format_tags_(other.identifier_format_tags_),
      identifier_count_(other.identifier_count_) {}

std::string UrlTemplate::BuildUri(const std::string& representation_id,
                                  int32_t segment_number,
                                  int32_t bandwidth,
                                  int32_t time_val) const {
  DCHECK(segment_number >= 0);
  DCHECK(bandwidth > 0);
  DCHECK(time_val >= 0);
  std::string builder;
  char buf[kUrlBuilderBufSize];
  for (int i = 0; i < identifier_count_; i++) {
    builder.append(url_pieces_[i]);
    if (identifiers_[i] == REPRESENTATION_ID) {
      builder.append(representation_id);
    } else if (identifiers_[i] == NUMBER_ID) {
      snprintf(buf, kUrlBuilderBufSize, identifier_format_tags_[i].c_str(),
               segment_number);
      builder.append(buf);
    } else if (identifiers_[i] == BANDWIDTH_ID) {
      snprintf(buf, kUrlBuilderBufSize, identifier_format_tags_[i].c_str(),
               bandwidth);
      builder.append(buf);
    } else if (identifiers_[i] == TIME_ID) {
      snprintf(buf, kUrlBuilderBufSize, identifier_format_tags_[i].c_str(),
               time_val);
      builder.append(buf);
    }
  }
  builder.append(url_pieces_[identifier_count_]);
  return builder;
}

// Implement template based segment url construction based on section 5.3.9.4.4
int32_t UrlTemplate::ParseTemplate(
    const std::string& template_str,
    std::vector<std::string>* url_pieces,
    std::vector<int32_t>* identifiers,
    std::vector<std::string>* identifier_format_tags) {
  url_pieces->push_back("");
  // Reserve a slot for the next identifier and associated tag.
  identifiers->push_back(0);
  identifier_format_tags->push_back("");
  int template_index = 0;
  int identifier_count = 0;
  // Start scanning the template string.
  while (template_index < template_str.length()) {
    int dollarIndex = template_str.find("$", template_index);
    std::string sub = template_str.substr(template_index);
    if (dollarIndex == std::string::npos) {
      // No $, consume remaining string into url_pieces.
      (*url_pieces)[identifier_count].append(sub);
      template_index = template_str.length();
    } else if (dollarIndex != template_index) {
      // $ appears down the road, consume non tag chars into url_pieces and
      // position template_index to the $ for next iteration
      (*url_pieces)[identifier_count].append(
          template_str.substr(template_index, dollarIndex - template_index));
      template_index = dollarIndex;
    } else if (sub.find(kEscapedDollar) == 0) {
      // $ appears but is immediately followed by another. Escape it and
      // continue.
      (*url_pieces)[identifier_count].append("$");
      template_index += 2;
    } else {
      // $ appears, not escaped.  Extract the identifier name.
      int secondIndex = template_str.find("$", template_index + 1);
      if (secondIndex < 0) {
        secondIndex = template_str.length();
      }
      std::string identifier = template_str.substr(
          template_index + 1, secondIndex - (template_index + 1));
      if (identifier.compare(kRepresentation) == 0) {
        // Format for representation is fixed.
        (*identifiers)[identifier_count] = REPRESENTATION_ID;
      } else {
        // Look for optional format tag at end of identifier. If present,
        // extract.
        int formatTagIndex = identifier.find("%0");
        std::string formatTag = kDefaultFormatTag;
        if (formatTagIndex != std::string::npos) {
          formatTag = identifier.substr(formatTagIndex);
          if (!(formatTag[formatTag.length() - 1] == 'd')) {
            formatTag.append("d");
          }
          identifier = identifier.substr(0, formatTagIndex);
          // Make sure only digits appears between %0 and trailing d.
          for (std::string::iterator it = formatTag.begin() + 1,
                                     end = formatTag.end() - 1;
               it != end; ++it) {
            if (!std::isdigit(*it)) {
              formatTag = kDefaultFormatTag;
              break;
            }
          }
        }
        if (identifier.compare(kNumber) == 0) {
          (*identifiers)[identifier_count] = NUMBER_ID;
        } else if (identifier.compare(kBandwidth) == 0) {
          (*identifiers)[identifier_count] = BANDWIDTH_ID;
        } else if (identifier.compare(kTime) == 0) {
          (*identifiers)[identifier_count] = TIME_ID;
        } else {
          // Not one of our known identifiers.
          CHECK(false);
        }
        (*identifier_format_tags)[identifier_count] = formatTag;
      }
      identifier_count++;
      url_pieces->push_back("");
      identifiers->push_back(0);
      identifier_format_tags->push_back("");
      template_index = secondIndex + 1;
    }
  }
  return identifier_count;
}

UrlTemplate& UrlTemplate::operator=(const UrlTemplate& other) {
  url_pieces_ = other.url_pieces_;
  identifiers_ = other.identifiers_;
  identifier_format_tags_ = other.identifier_format_tags_;
  identifier_count_ = other.identifier_count_;
  return *this;
}

}  // namespace mpd

}  // namespace ndash
