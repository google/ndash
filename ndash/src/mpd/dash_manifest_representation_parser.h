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

#ifndef NDASH_MPD_DASH_MANIFEST_REPRESENTATION_PARSER_
#define NDASH_MPD_DASH_MANIFEST_REPRESENTATION_PARSER_

#include <libxml/xmlreader.h>

#include <memory>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/strings/string_piece.h"
#include "drm/scheme_init_data.h"
#include "mpd/content_protections_builder.h"
#include "mpd/media_presentation_description.h"
#include "mpd/segment_list.h"
#include "mpd/segment_template.h"
#include "mpd/segment_timeline_element.h"
#include "mpd/single_segment_base.h"

namespace ndash {

namespace mpd {

// A parser of media presentation description files.
class MediaPresentationDescriptionParser {
 public:
  MediaPresentationDescriptionParser(std::string content_id = "");

  scoped_refptr<MediaPresentationDescription> Parse(
      const std::string& connection_url,
      base::StringPiece xml) const;

 private:
  std::string content_id_;

  scoped_refptr<MediaPresentationDescription> ParseMediaPresentationDescription(
      xmlTextReaderPtr reader,
      const std::string& base_url) const;

  scoped_refptr<MediaPresentationDescription> BuildMediaPresentationDescription(
      int64_t availability_start_time,
      int64_t duration_ms,
      int64_t min_buffer_time_ms,
      bool dynamic,
      int64_t min_update_time_ms,
      int64_t time_shift_buffer_depth_ms,
      std::unique_ptr<DescriptorType> utc_timing,
      const std::string& location,
      std::vector<std::unique_ptr<Period>>* periods,
      std::vector<std::unique_ptr<DescriptorType>>* supplemental_properties,
      std::vector<std::unique_ptr<DescriptorType>>* essential_properties) const;

  std::unique_ptr<DescriptorType> ParseDescriptorType(xmlNodePtr node) const;

  std::unique_ptr<DescriptorType> BuildDescriptorTypeElement(
      const std::string& scheme_id_uri,
      const std::string& value,
      const std::string& id) const;

  std::pair<std::unique_ptr<Period>, int64_t> ParsePeriod(
      xmlTextReaderPtr reader,
      const std::string& base_url,
      int64_t default_start_ms) const;

  std::unique_ptr<Period> BuildPeriod(
      std::string id,
      int64_t start_ms,
      std::vector<std::unique_ptr<AdaptationSet>>* adaptation_sets,
      std::unique_ptr<SegmentBase> segment_base,
      std::vector<std::unique_ptr<DescriptorType>>* supplemental_properties)
      const;

  // AdaptationSet parsing.

  // Returns null on parsing error.
  std::unique_ptr<AdaptationSet> ParseAdaptationSet(
      xmlTextReaderPtr reader,
      const std::string& base_url,
      SegmentBase* segment_base) const;

  std::unique_ptr<AdaptationSet> BuildAdaptationSet(
      int32_t id,
      AdaptationType contentType,
      std::vector<std::unique_ptr<Representation>>* representations,
      std::vector<std::unique_ptr<ContentProtection>>* contentProtections,
      std::unique_ptr<SegmentBase> segment_base,
      std::vector<std::unique_ptr<DescriptorType>>* supplemental_properties,
      std::vector<std::unique_ptr<DescriptorType>>* essential_properties) const;

  AdaptationType ParseContentType(xmlNodePtr node) const;

  static AdaptationType GetContentType(const Representation* representation);

  // Parses a ContentProtection element.
  std::unique_ptr<ContentProtection> ParseContentProtection(
      xmlTextReaderPtr reader) const;

  std::unique_ptr<ContentProtection> BuildContentProtection(
      const std::string& scheme_id_uri,
      const util::Uuid& uuid,
      std::unique_ptr<drm::SchemeInitData> data) const;

  // Representation parsing.

  std::unique_ptr<Representation> ParseRepresentation(
      xmlTextReaderPtr reader,
      const std::string& base_url,
      const std::string& adaptation_set_mime_type,
      const std::string& adaptation_set_codecs,
      int32_t adaptation_set_width,
      int32_t adaptation_set_height,
      float adaptation_set_frame_rate,
      int32_t max_playout_rate,
      int32_t adaptation_set_audio_channels,
      int32_t adaptation_set_audio_samplingRate,
      const std::string& adaptation_set_language,
      SegmentBase* segment_base,
      ContentProtectionsBuilder* content_protections_builder) const;

  util::Format BuildFormat(const std::string& id,
                           const std::string& mimeType,
                           int32_t width,
                           int32_t height,
                           float frame_rate,
                           int32_t max_playout_rate,
                           int32_t audio_channels,
                           int32_t audio_sampling_rate,
                           int32_t bandwidth,
                           const std::string& language,
                           const std::string& codecs) const;

  std::unique_ptr<Representation> BuildRepresentation(
      const std::string& content_id,
      int revision_id,
      util::Format format,
      SegmentBase* segment_base,
      std::vector<std::unique_ptr<DescriptorType>>* supplemental_properties,
      std::vector<std::unique_ptr<DescriptorType>>* essential_properties) const;

  std::unique_ptr<Representation> BuildRepresentation(
      const std::string& content_id,
      int revision_id,
      util::Format format,
      std::unique_ptr<SegmentBase> segment_base_override,
      std::vector<std::unique_ptr<DescriptorType>>* supplemental_properties,
      std::vector<std::unique_ptr<DescriptorType>>* essential_properties) const;

  // SegmentBase, SegmentList and SegmentTemplate parsing.

  // Returns null if parsing error occurred.
  std::unique_ptr<SingleSegmentBase> ParseSegmentBase(
      xmlTextReaderPtr reader,
      const std::string& base_url,
      const SingleSegmentBase* parent) const;

  std::unique_ptr<SingleSegmentBase> BuildSingleSegmentBase(
      std::unique_ptr<RangedUri> initialization,
      int64_t timescale,
      int64_t presentation_time_offset,
      std::unique_ptr<std::string> base_url,
      int64_t index_start,
      int64_t index_length) const;

  // Parse a byte range returning true on success, false on failure. Places
  // the start and length of the parsed range into start and length output
  // vars.
  bool ParseRange(const std::string& str,
                  int64_t* start,
                  int64_t* length) const;

  // Returns null if parsing error occurred.
  std::unique_ptr<SegmentList> ParseSegmentList(xmlTextReaderPtr reader,
                                                const std::string& base_url,
                                                SegmentList* parent) const;

  std::unique_ptr<SegmentList> BuildSegmentList(
      std::unique_ptr<std::string> base_url,
      std::unique_ptr<RangedUri> initialization,
      uint64_t timescale,
      uint64_t presentation_time_offset,
      int32_t start_number,
      uint64_t duration,
      std::unique_ptr<std::vector<SegmentTimelineElement>> timeline,
      std::unique_ptr<std::vector<RangedUri>> segments,
      SegmentList* parent) const;

  // Returns null if parsing error occurred.
  std::unique_ptr<SegmentTemplate> ParseSegmentTemplate(
      xmlTextReaderPtr reader,
      const std::string& base_url,
      SegmentTemplate* parent) const;

  std::unique_ptr<SegmentTemplate> BuildSegmentTemplate(
      std::unique_ptr<std::string> base_url,
      std::unique_ptr<RangedUri> initialization,
      int64_t timescale,
      int64_t presentation_time_offset,
      int32_t start_number,
      int64_t duration,
      std::unique_ptr<std::vector<SegmentTimelineElement>> timeline,
      std::unique_ptr<UrlTemplate> initialization_template,
      std::unique_ptr<UrlTemplate> media_template,
      SegmentTemplate* parent) const;

  std::unique_ptr<std::vector<SegmentTimelineElement>> ParseSegmentTimeline(
      xmlTextReaderPtr reader) const;

  SegmentTimelineElement BuildSegmentTimelineElement(int64_t elapsed_time,
                                                     int64_t duration) const;

  std::unique_ptr<UrlTemplate> ParseUrlTemplate(
      xmlTextReaderPtr reader,
      const std::string& name,
      std::unique_ptr<UrlTemplate> default_value) const;

  std::unique_ptr<RangedUri> ParseInitialization(
      xmlTextReaderPtr reader,
      const std::string* base_url) const;

  std::unique_ptr<RangedUri> ParseSegmentUrl(xmlTextReaderPtr reader,
                                             const std::string* base_url) const;

  std::unique_ptr<RangedUri> ParseRangedUrl(
      xmlTextReaderPtr reader,
      const std::string* base_url,
      const std::string& url_attribute,
      const std::string& range_attribute) const;

  std::unique_ptr<RangedUri> BuildRangedUri(const std::string* base_url,
                                            const std::string& urlText,
                                            int64_t range_start,
                                            int64_t range_length) const;

  // AudioChannelConfiguration parsing
  int32_t ParseAudioChannelConfiguration(xmlTextReaderPtr reader) const;

  // Utility methods

  static bool ParseFrameRate(xmlNodePtr node,
                             double* out,
                             double default_value);

  static std::string GetAttributeValue(xmlNodePtr node,
                                       const std::string& name,
                                       std::string default_value = "");

  static int64_t ParseDuration(xmlNodePtr node,
                               const std::string& name,
                               int64_t default_value);

  static int64_t ParseDateTime(xmlNodePtr node,
                               const std::string& name,
                               int64_t default_value);

  static std::string ParseBaseUrl(xmlTextReaderPtr reader,
                                  const std::string& parent_base_url);

  static bool ParseInt(xmlNodePtr node,
                       const std::string& name,
                       int32_t* out,
                       int32_t default_value = -1);

  static bool ParseLong(xmlNodePtr node,
                        const std::string& name,
                        int64_t* out,
                        int64_t default_value = -1);

  // Convert a string to an int32_t.
  // NOTE: Strings representing values outside the range of int32_t will not
  // produce an error.
  static int32_t ParseInt(const std::string& str, int32_t* out);

  // Convert a string to an int64_t.
  static int64_t ParseLong(const std::string& str, int64_t* out);

  static std::string ParseString(xmlNodePtr node,
                                 const std::string& name,
                                 const std::string& default_value);

  // Returns true if the current node the given reader is pointing to equals
  // 'name'. False otherwise.
  static bool CurrentNodeNameEquals(xmlTextReaderPtr reader,
                                    const std::string& name);

  // Returns true if the provided node equals 'name'. False otherwise.
  static bool NodeNameEquals(xmlNodePtr node, const std::string& name);

  // This method will advance the reader to the current node's next sibling,
  // returning the current node's text contents (first text node only).
  static std::string NextText(xmlTextReaderPtr reader);

  // Utility methods.

  // Checks two languages for consistency, returning the consistent language, or
  // an empty string if the languages are inconsistent.
  // Two languages are consistent if they are equal, or if one is empty.
  std::string CheckLanguageConsistency(
      const std::string& first_language,
      const std::string& second_language) const;

  // Checks two adaptation set content types for consistency, returning the
  // consistent type, or -1 if the types are inconsistent.
  // Two types are consistent if they are equal, or if one is
  // AdaptationSet::TYPE_UNKNOWN
  // Where one of the types is AdaptationSet::TYPE_UNKNOWN, the other is
  // returned.
  AdaptationType CheckContentTypeConsistency(AdaptationType first_type,
                                             AdaptationType second_type) const;
};

}  // namespace mpd

}  // namespace ndash

#endif  // NDASH_MPD_DASH_MANIFEST_REPRESENTATION_PARSER_
