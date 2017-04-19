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

#include "mpd/dash_manifest_representation_parser.h"

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <locale>

#include "base/base64.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "drm/scheme_init_data.h"
#include "util/mime_types.h"
#include "util/uri_util.h"
#include "util/util.h"

// A macro to convert a char* to xmlChar*
#define TO_XMLCHAR(x) reinterpret_cast<const xmlChar*>(x)
// A macro to convert a xmlChar* to char*
#define FROM_XMLCHAR(x) reinterpret_cast<const char*>(x)

namespace ndash {

namespace mpd {

MediaPresentationDescriptionParser::MediaPresentationDescriptionParser(
    std::string content_id)
    : content_id_(content_id) {}

scoped_refptr<MediaPresentationDescription>
MediaPresentationDescriptionParser::Parse(const std::string& connection_url,
                                          base::StringPiece xml) const {
  std::unique_ptr<xmlTextReader, void (*)(xmlTextReaderPtr)> reader(
      xmlReaderForMemory(xml.data(), xml.length(), connection_url.c_str(),
                         nullptr, 0),
      xmlFreeTextReader);

  scoped_refptr<MediaPresentationDescription> mpd;

  int ret = xmlTextReaderRead(reader.get());
  if (ret == 1) {
    if (CurrentNodeNameEquals(reader.get(), "MPD")) {
      mpd = ParseMediaPresentationDescription(reader.get(), connection_url);
    }
  }

  return mpd;
}

scoped_refptr<MediaPresentationDescription>
MediaPresentationDescriptionParser::ParseMediaPresentationDescription(
    xmlTextReaderPtr reader,
    const std::string& base_url) const {
  xmlNodePtr node = xmlTextReaderCurrentNode(reader);

  int64_t availabilityStartTime =
      ParseDateTime(node, "availabilityStartTime", -1);
  int64_t duration_ms = ParseDuration(node, "mediaPresentationDuration", -1);
  int64_t minBufferTimeMs = ParseDuration(node, "minBufferTime", -1);
  xmlChar* type_string = xmlGetProp(node, TO_XMLCHAR("type"));
  bool dynamic = false;
  if (type_string != nullptr) {
    dynamic =
        xmlStrEqual(TO_XMLCHAR("dynamic"), type_string) == 1 ? true : false;
  }
  xmlFree(type_string);

  int64_t min_update_time_ms =
      (dynamic) ? ParseDuration(node, "minimumUpdatePeriod", -1) : -1;
  int64_t time_shift_buffer_depth_ms =
      (dynamic) ? ParseDuration(node, "timeShiftBufferDepth", -1) : -1;
  std::unique_ptr<DescriptorType> utcTiming;
  std::string location;

  std::vector<std::unique_ptr<Period>> periods;
  std::vector<std::unique_ptr<DescriptorType>> supplemental_properties;
  std::vector<std::unique_ptr<DescriptorType>> essential_properties;
  int64_t next_period_start_ms = dynamic ? -1 : 0;
  bool seen_early_access_period = false;
  bool seen_first_base_Url = false;
  std::string base_url_override(base_url);

  int ret;
  int parent_depth = xmlTextReaderDepth(reader);
  int depth;
  do {
    ret = xmlTextReaderRead(reader);
    if (ret != 1) {
      break;
    }
    depth = xmlTextReaderDepth(reader);
    xmlNodePtr child = xmlTextReaderCurrentNode(reader);
    if (CurrentNodeNameEquals(reader, "BaseURL")) {
      if (!seen_first_base_Url) {
        base_url_override = ParseBaseUrl(reader, base_url_override);
        seen_first_base_Url = true;
      }
    } else if (CurrentNodeNameEquals(reader, "SupplementalProperty")) {
      supplemental_properties.push_back(ParseDescriptorType(child));
    } else if (CurrentNodeNameEquals(reader, "EssentialProperty")) {
      essential_properties.push_back(ParseDescriptorType(child));
    } else if (CurrentNodeNameEquals(reader, "UTCTiming")) {
      utcTiming = ParseDescriptorType(child);
    } else if (CurrentNodeNameEquals(reader, "Location")) {
      location = NextText(reader);
    } else if (CurrentNodeNameEquals(reader, "Period") &&
               !seen_early_access_period) {
      std::pair<std::unique_ptr<Period>, int64_t> period_with_duration_ms =
          ParsePeriod(reader, base_url_override, next_period_start_ms);
      Period* period = period_with_duration_ms.first.get();
      if (period == nullptr) {
        LOG(FATAL) << "Could not parse period.";
        return nullptr;
      }
      if (period->GetStartMs() == -1) {
        if (dynamic) {
          // This is an early access period. Ignore it. All subsequent periods
          // must also be
          // early access.
          seen_early_access_period = true;
        } else {
          LOG(FATAL) << "Unable to determine start of period.";
          return nullptr;
        }
      } else {
        int64_t period_duration_ms = period_with_duration_ms.second;
        next_period_start_ms = period_duration_ms == -1
                                   ? -1
                                   : period->GetStartMs() + period_duration_ms;
        periods.push_back(std::move(period_with_duration_ms.first));
      }
    }
  } while (depth > parent_depth);

  if (duration_ms == -1) {
    if (next_period_start_ms != -1) {
      // If we know the end time of the final period, we can use it as the
      // duration.
      duration_ms = next_period_start_ms;
    } else if (!dynamic) {
      return nullptr;
    }
  }

  if (periods.size() == 0) {
    // No periods.
    return nullptr;
  }
  return BuildMediaPresentationDescription(
      availabilityStartTime, duration_ms, minBufferTimeMs, dynamic,
      min_update_time_ms, time_shift_buffer_depth_ms, std::move(utcTiming),
      location, &periods, &supplemental_properties, &essential_properties);
}

scoped_refptr<MediaPresentationDescription>
MediaPresentationDescriptionParser::BuildMediaPresentationDescription(
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
    std::vector<std::unique_ptr<DescriptorType>>* essential_properties) const {
  return scoped_refptr<MediaPresentationDescription>(
      new MediaPresentationDescription(
          availability_start_time, duration_ms, min_buffer_time_ms, dynamic,
          min_update_time_ms, time_shift_buffer_depth_ms, std::move(utc_timing),
          location, periods, supplemental_properties, essential_properties));
}

std::unique_ptr<DescriptorType>
MediaPresentationDescriptionParser::ParseDescriptorType(xmlNodePtr node) const {
  xmlChar* scheme_prop = xmlGetProp(node, TO_XMLCHAR("schemeIdUri"));
  xmlChar* value_prop = xmlGetProp(node, TO_XMLCHAR("value"));
  xmlChar* id_prop = xmlGetProp(node, TO_XMLCHAR("id"));
  std::string scheme_id;
  if (scheme_prop != nullptr) {
    scheme_id = std::string(FROM_XMLCHAR(scheme_prop));
    xmlFree(scheme_prop);
  }
  std::string value;
  if (value_prop != nullptr) {
    value = std::string(FROM_XMLCHAR(value_prop));
    xmlFree(value_prop);
  }
  std::string id;
  if (id_prop != nullptr) {
    scheme_id = std::string(FROM_XMLCHAR(id_prop));
    xmlFree(id_prop);
  }
  return BuildDescriptorTypeElement(scheme_id, value, id);
}

std::unique_ptr<DescriptorType>
MediaPresentationDescriptionParser::BuildDescriptorTypeElement(
    const std::string& scheme_id_uri,
    const std::string& value,
    const std::string& id) const {
  return std::unique_ptr<DescriptorType>(
      new DescriptorType(scheme_id_uri, value, id));
}

std::pair<std::unique_ptr<Period>, int64_t>
MediaPresentationDescriptionParser::ParsePeriod(xmlTextReaderPtr reader,
                                                const std::string& base_url,
                                                int64_t defaultStartMs) const {
  xmlNodePtr node = xmlTextReaderCurrentNode(reader);
  std::string id = GetAttributeValue(node, "id");
  int64_t start_ms = ParseDuration(node, "start", defaultStartMs);
  int64_t duration_ms = ParseDuration(node, "duration", -1);

  std::unique_ptr<SegmentBase> segment_base;
  std::vector<std::unique_ptr<AdaptationSet>> adaptation_sets;
  std::vector<std::unique_ptr<DescriptorType>> supplemental_properties;

  std::string base_url_override(base_url);
  bool seen_first_base_url = false;
  int ret;
  int parent_depth = xmlTextReaderDepth(reader);
  int depth;
  do {
    ret = xmlTextReaderRead(reader);
    if (ret != 1) {
      break;
    }
    depth = xmlTextReaderDepth(reader);
    xmlNodePtr child = xmlTextReaderCurrentNode(reader);
    if (NodeNameEquals(child, "BaseURL")) {
      if (!seen_first_base_url) {
        base_url_override = ParseBaseUrl(reader, base_url_override);
        seen_first_base_url = true;
      }
    } else if (CurrentNodeNameEquals(reader, "SupplementalProperty")) {
      supplemental_properties.push_back(ParseDescriptorType(child));
    } else if (NodeNameEquals(child, "AdaptationSet")) {
      std::unique_ptr<AdaptationSet> adaptation_set =
          ParseAdaptationSet(reader, base_url_override, segment_base.get());
      if (adaptation_set.get() == nullptr) {
        return std::pair<std::unique_ptr<Period>, int64_t>(nullptr, -1);
      }
      adaptation_sets.push_back(std::move(adaptation_set));
    } else if (NodeNameEquals(child, "SegmentBase")) {
      segment_base = ParseSegmentBase(reader, base_url_override, nullptr);
      if (segment_base.get() == nullptr) {
        return std::pair<std::unique_ptr<Period>, int64_t>(nullptr, -1);
      }
    } else if (NodeNameEquals(child, "SegmentList")) {
      segment_base = ParseSegmentList(reader, base_url_override, nullptr);
      if (segment_base.get() == nullptr) {
        return std::pair<std::unique_ptr<Period>, int64_t>(nullptr, -1);
      }
    } else if (NodeNameEquals(child, "SegmentTemplate")) {
      segment_base = ParseSegmentTemplate(reader, base_url_override, nullptr);
      if (segment_base.get() == nullptr) {
        return std::pair<std::unique_ptr<Period>, int64_t>(nullptr, -1);
      }
    }
  } while (depth > parent_depth);

  // The Period takes ownership of the segment base we created at this level
  // (if any was created).
  return std::pair<std::unique_ptr<Period>, int64_t>(
      BuildPeriod(id, start_ms, &adaptation_sets, std::move(segment_base),
                  &supplemental_properties),
      duration_ms);
}

std::unique_ptr<Period> MediaPresentationDescriptionParser::BuildPeriod(
    std::string id,
    int64_t start_ms,
    std::vector<std::unique_ptr<AdaptationSet>>* adaptation_sets,
    std::unique_ptr<SegmentBase> segment_base,
    std::vector<std::unique_ptr<DescriptorType>>* supplemental_properties)
    const {
  return std::unique_ptr<Period>(new Period(id, start_ms, adaptation_sets,
                                            std::move(segment_base),
                                            supplemental_properties));
}

// AdaptationSet parsing.

std::unique_ptr<AdaptationSet>
MediaPresentationDescriptionParser::ParseAdaptationSet(
    xmlTextReaderPtr reader,
    const std::string& base_url,
    SegmentBase* segment_base) const {
  xmlNodePtr node = xmlTextReaderCurrentNode(reader);
  int32_t id;
  if (!ParseInt(node, "id", &id, -1)) {
    LOG(INFO) << "Failed to parse adaptation set 'id' attribute";
    return nullptr;
  }
  AdaptationType content_type = ParseContentType(node);
  std::vector<std::unique_ptr<DescriptorType>> supplemental_properties;
  std::vector<std::unique_ptr<DescriptorType>> essential_properties;

  std::string mime_type = GetAttributeValue(node, "mimeType");
  std::string codecs = GetAttributeValue(node, "codecs");
  int32_t width;
  int32_t height;
  if (!ParseInt(node, "width", &width, -1)) {
    LOG(INFO) << "Failed to parse adaptation set 'width' attribute";
    return nullptr;
  }
  if (!ParseInt(node, "height", &height, -1)) {
    LOG(INFO) << "Failed to parse adaptation set 'height' attribute";
    return nullptr;
  }
  double frame_rate;
  if (!ParseFrameRate(node, &frame_rate, -1)) {
    return nullptr;
  }
  int32_t max_playout_rate;
  if (!ParseInt(node, "maxPlayoutRate", &max_playout_rate, 1)) {
    return nullptr;
  }
  int32_t audio_channels = -1;
  int32_t audio_sampling_rate;
  if (!ParseInt(node, "audioSamplingRate", &audio_sampling_rate, -1)) {
    LOG(INFO) << "Failed to parse adaptation set 'audioSamplingRate' attribute";
    return nullptr;
  }
  std::string language = GetAttributeValue(node, "lang");

  ContentProtectionsBuilder content_protections_builder;
  std::unique_ptr<SegmentBase> segment_base_override;
  std::vector<std::unique_ptr<Representation>> representations;

  std::string base_url_override(base_url);
  bool seen_first_base_Url = false;
  int ret;
  int parent_depth = xmlTextReaderDepth(reader);
  int depth;
  do {
    ret = xmlTextReaderRead(reader);
    if (ret != 1) {
      break;
    }
    depth = xmlTextReaderDepth(reader);
    xmlNodePtr child = xmlTextReaderCurrentNode(reader);
    if (NodeNameEquals(child, "BaseURL")) {
      if (!seen_first_base_Url) {
        base_url_override = ParseBaseUrl(reader, base_url_override);
        seen_first_base_Url = true;
      }
    } else if (CurrentNodeNameEquals(reader, "SupplementalProperty")) {
      supplemental_properties.push_back(ParseDescriptorType(child));
    } else if (CurrentNodeNameEquals(reader, "EssentialProperty")) {
      essential_properties.push_back(ParseDescriptorType(child));
    } else if (NodeNameEquals(child, "ContentProtection")) {
      std::unique_ptr<ContentProtection> contentProtection =
          ParseContentProtection(reader);
      if (contentProtection.get() != nullptr) {
        content_protections_builder.AddAdaptationSetProtection(
            std::move(contentProtection));
      }
    } else if (NodeNameEquals(child, "ContentComponent")) {
      std::string child_lang = GetAttributeValue(child, "lang");
      language = CheckLanguageConsistency(language, child_lang);
      content_type =
          CheckContentTypeConsistency(content_type, ParseContentType(child));
    } else if (NodeNameEquals(child, "Representation")) {
      std::unique_ptr<Representation> representation = ParseRepresentation(
          reader, base_url_override, mime_type, codecs, width, height,
          frame_rate, max_playout_rate, audio_channels, audio_sampling_rate,
          language, segment_base, &content_protections_builder);
      if (representation.get() == nullptr) {
        return nullptr;
      }
      content_protections_builder.EndRepresentation();
      content_type = CheckContentTypeConsistency(
          content_type, GetContentType(representation.get()));
      representations.push_back(std::move(representation));
    } else if (NodeNameEquals(child, "AudioChannelConfiguration")) {
      audio_channels = ParseAudioChannelConfiguration(reader);
    } else if (NodeNameEquals(child, "SegmentBase")) {
      segment_base_override =
          ParseSegmentBase(reader, base_url_override,
                           static_cast<SingleSegmentBase*>(segment_base));
      if (segment_base_override.get() == nullptr) {
        return nullptr;
      }
      segment_base = segment_base_override.get();
    } else if (NodeNameEquals(child, "SegmentList")) {
      segment_base_override = ParseSegmentList(
          reader, base_url_override, static_cast<SegmentList*>(segment_base));
      if (segment_base_override.get() == nullptr) {
        return nullptr;
      }
      segment_base = segment_base_override.get();
    } else if (NodeNameEquals(child, "SegmentTemplate")) {
      segment_base_override =
          ParseSegmentTemplate(reader, base_url_override,
                               static_cast<SegmentTemplate*>(segment_base));
      if (segment_base_override.get() == nullptr) {
        return nullptr;
      }
      segment_base = segment_base_override.get();
    }
  } while (depth > parent_depth);

  // The AdaptationSet takes ownership of the segment base we created at this
  // level (if any was created).
  std::unique_ptr<std::vector<std::unique_ptr<ContentProtection>>>
      content_protections = content_protections_builder.Build();
  return BuildAdaptationSet(id, content_type, &representations,
                            content_protections.get(),
                            std::move(segment_base_override),
                            &supplemental_properties, &essential_properties);
}

std::unique_ptr<AdaptationSet>
MediaPresentationDescriptionParser::BuildAdaptationSet(
    int32_t id,
    AdaptationType content_type,
    std::vector<std::unique_ptr<Representation>>* representations,
    std::vector<std::unique_ptr<ContentProtection>>* content_protections,
    std::unique_ptr<SegmentBase> segment_base,
    std::vector<std::unique_ptr<DescriptorType>>* supplemental_properties,
    std::vector<std::unique_ptr<DescriptorType>>* essential_properties) const {
  return std::unique_ptr<AdaptationSet>(new AdaptationSet(
      id, content_type, representations, content_protections,
      std::move(segment_base), supplemental_properties, essential_properties));
}

AdaptationType MediaPresentationDescriptionParser::ParseContentType(
    xmlNodePtr node) const {
  std::string content_type = GetAttributeValue(node, "contentType");
  if (content_type.empty()) {
    return AdaptationType::UNKNOWN;
  } else {
    if (util::kBaseTypeAudio == content_type) {
      return AdaptationType::AUDIO;
    } else if (util::kBaseTypeVideo == content_type) {
      return AdaptationType::VIDEO;
    } else if (util::kBaseTypeText == content_type) {
      return AdaptationType::TEXT;
    }
    return AdaptationType::UNKNOWN;
  }
}

AdaptationType MediaPresentationDescriptionParser::GetContentType(
    const Representation* representation) {
  DCHECK(representation != nullptr);
  std::string mime_type = representation->GetFormat().GetMimeType();
  if (mime_type.empty()) {
    return AdaptationType::UNKNOWN;
  } else if (util::MimeTypes::IsVideo(mime_type)) {
    return AdaptationType::VIDEO;
  } else if (util::MimeTypes::IsAudio(mime_type)) {
    return AdaptationType::AUDIO;
  } else if (util::MimeTypes::IsText(mime_type)) {
    return AdaptationType::TEXT;
  } else if (util::kApplicationMP4 == mime_type) {
    // The representation uses mp4 but does not contain video or audio. Use
    // codecs to determine
    // whether the container holds text.
    std::string codecs = representation->GetFormat().GetCodecs();
    if ("stpp" == codecs || "wvtt" == codecs) {
      return AdaptationType::TEXT;
    }
  }
  return AdaptationType::UNKNOWN;
}

std::unique_ptr<ContentProtection>
MediaPresentationDescriptionParser::ParseContentProtection(
    xmlTextReaderPtr reader) const {
  xmlNodePtr node = xmlTextReaderCurrentNode(reader);
  std::string scheme_id_uri = GetAttributeValue(node, "schemeIdUri");
  util::Uuid uuid;
  std::unique_ptr<drm::SchemeInitData> data;
  bool seen_pssh_element = false;
  int ret;
  int parent_depth = xmlTextReaderDepth(reader);
  int depth;
  do {
    ret = xmlTextReaderRead(reader);
    if (ret != 1) {
      break;
    }
    depth = xmlTextReaderDepth(reader);
    xmlNodePtr child = xmlTextReaderCurrentNode(reader);
    // The cenc:pssh element is defined in 23001-7:2015.
    if (NodeNameEquals(child, "cenc:pssh")) {
      seen_pssh_element = true;
      std::string base_64 = NextText(reader);
      std::string decoded;
      base::Base64Decode(base_64, &decoded);
      size_t num_bytes = decoded.size();
      std::unique_ptr<char[]> bytes =
          std::unique_ptr<char[]>(new char[num_bytes]);
      std::memcpy(bytes.get(), decoded.c_str(), num_bytes);
      data.reset(new drm::SchemeInitData(util::kVideoMP4, std::move(bytes),
                                         num_bytes));
      // TODO(rmrossi): Implement PsshAtomUtil to construct the uuid.
      NOTREACHED();
      // uuid = PsshAtomUtil.parseUuid(data.data);
    }
  } while (depth > parent_depth);

  if (seen_pssh_element && uuid.is_empty()) {
    // Skipped unsupported ContentProtection element
    return std::unique_ptr<ContentProtection>(nullptr);
  }
  return BuildContentProtection(scheme_id_uri, uuid, std::move(data));
}

std::unique_ptr<ContentProtection>
MediaPresentationDescriptionParser::BuildContentProtection(
    const std::string& scheme_id_uri,
    const util::Uuid& uuid,
    std::unique_ptr<drm::SchemeInitData> data) const {
  return std::unique_ptr<ContentProtection>(
      new ContentProtection(scheme_id_uri, uuid, std::move(data)));
}

// Representation parsing.
std::unique_ptr<Representation>
MediaPresentationDescriptionParser::ParseRepresentation(
    xmlTextReaderPtr reader,
    const std::string& base_url,
    const std::string& adaptation_set_mime_type,
    const std::string& adaptation_set_codecs,
    int32_t adaptation_set_width,
    int32_t adaptation_set_height,
    float adaptation_set_frame_rate,
    int32_t adaptation_set_max_playout_rate,
    int32_t adaptation_set_audio_channels,
    int32_t adaptation_set_audio_samplingRate,
    const std::string& adaptation_set_language,
    SegmentBase* segment_base,
    ContentProtectionsBuilder* content_protections_builder) const {
  std::vector<std::unique_ptr<DescriptorType>> supplemental_properties;
  std::vector<std::unique_ptr<DescriptorType>> essential_properties;

  xmlNodePtr node = xmlTextReaderCurrentNode(reader);

  // RangedUris constructed under this representation want to share memory
  // for a common base url. Take a copy and ownership will go with the
  // segment base.

  std::string id = GetAttributeValue(node, "id");
  int32_t bandwidth;
  if (!ParseInt(node, "bandwidth", &bandwidth)) {
    LOG(INFO) << "Failed to parse representation 'bandwidth' attribute";
    return nullptr;
  }

  std::string mime_type =
      GetAttributeValue(node, "mimeType", adaptation_set_mime_type);
  std::string codecs = GetAttributeValue(node, "codecs", adaptation_set_codecs);

  int32_t width;
  int32_t height;
  if (!ParseInt(node, "width", &width, adaptation_set_width)) {
    LOG(INFO) << "Failed to parse representation 'width' attribute";
    return nullptr;
  }
  if (!ParseInt(node, "height", &height, adaptation_set_height)) {
    LOG(INFO) << "Failed to parse representation 'height' attribute";
    return nullptr;
  }
  double frame_rate;
  if (!ParseFrameRate(node, &frame_rate, adaptation_set_frame_rate)) {
    LOG(INFO) << "Failed to parse representation 'frameRate' attribute";
    return nullptr;
  }
  int32_t max_playout_rate;
  if (!ParseInt(node, "maxPlayoutRate", &max_playout_rate,
                adaptation_set_max_playout_rate)) {
    LOG(INFO) << "Failed to parse representation 'maxPlayoutRate' attribute";
    return nullptr;
  }
  int32_t audio_channels = adaptation_set_audio_channels;
  int32_t audio_sampling_rate;
  if (!ParseInt(node, "audioSamplingRate", &audio_sampling_rate,
                adaptation_set_audio_samplingRate)) {
    LOG(INFO) << "Failed to parse representation 'audioSamplingRate' attribute";
    return nullptr;
  }
  std::string language = adaptation_set_language;

  std::unique_ptr<SegmentBase> segment_base_override;

  std::string base_url_override(base_url);
  bool seen_first_base_Url = false;
  int ret;
  int parent_depth = xmlTextReaderDepth(reader);
  int depth;
  do {
    ret = xmlTextReaderRead(reader);
    if (ret != 1) {
      break;
    }
    depth = xmlTextReaderDepth(reader);
    xmlNodePtr child = xmlTextReaderCurrentNode(reader);
    if (NodeNameEquals(child, "BaseURL")) {
      if (!seen_first_base_Url) {
        base_url_override = ParseBaseUrl(reader, base_url_override);
        seen_first_base_Url = true;
      }
    } else if (CurrentNodeNameEquals(reader, "SupplementalProperty")) {
      supplemental_properties.push_back(ParseDescriptorType(child));
    } else if (CurrentNodeNameEquals(reader, "EssentialProperty")) {
      essential_properties.push_back(ParseDescriptorType(child));
    } else if (NodeNameEquals(child, "AudioChannelConfiguration")) {
      audio_channels = ParseAudioChannelConfiguration(reader);
    } else if (NodeNameEquals(child, "SegmentBase")) {
      segment_base_override = ParseSegmentBase(
          reader, base_url_override,
          reinterpret_cast<const SingleSegmentBase*>(segment_base));
      if (segment_base_override.get() == nullptr) {
        return nullptr;
      }
      segment_base = segment_base_override.get();
    } else if (NodeNameEquals(child, "SegmentList")) {
      segment_base_override = ParseSegmentList(
          reader, base_url_override, static_cast<SegmentList*>(segment_base));
      if (segment_base_override.get() == nullptr) {
        return nullptr;
      }
      segment_base = segment_base_override.get();
    } else if (NodeNameEquals(child, "SegmentTemplate")) {
      segment_base_override =
          ParseSegmentTemplate(reader, base_url_override,
                               static_cast<SegmentTemplate*>(segment_base));
      if (segment_base_override.get() == nullptr) {
        return nullptr;
      }
      segment_base = segment_base_override.get();
    } else if (NodeNameEquals(child, "ContentProtection")) {
      std::unique_ptr<ContentProtection> contentProtection =
          ParseContentProtection(reader);
      if (contentProtection.get() != nullptr) {
        content_protections_builder->AddAdaptationSetProtection(
            std::move(contentProtection));
      }
    }
  } while (depth > parent_depth);

  util::Format format = BuildFormat(
      id, mime_type, width, height, frame_rate, max_playout_rate,
      audio_channels, audio_sampling_rate, bandwidth, language, codecs);
  if (segment_base == nullptr) {
    std::unique_ptr<std::string> new_base_url =
        std::unique_ptr<std::string>(new std::string(base_url_override));

    segment_base_override = std::unique_ptr<SegmentBase>(
        new SingleSegmentBase(std::move(new_base_url)));
    segment_base = segment_base_override.get();
  }

  if (segment_base_override.get() != nullptr) {
    // The representation will own its own segment base.
    return BuildRepresentation(content_id_, -1, format,
                               std::move(segment_base_override),
                               &supplemental_properties, &essential_properties);
  } else {
    // The representation will refer to a segment base owned by a higher level.
    return BuildRepresentation(content_id_, -1, format, segment_base,
                               &supplemental_properties, &essential_properties);
  }
}

util::Format MediaPresentationDescriptionParser::BuildFormat(
    const std::string& id,
    const std::string& mime_type,
    int32_t width,
    int32_t height,
    float frame_rate,
    int32_t max_playout_rate,
    int32_t audio_channels,
    int32_t audio_sampling_rate,
    int32_t bandwidth,
    const std::string& language,
    const std::string& codecs) const {
  std::string fixed_codecs = codecs;
  // b/31863242 - datagen is setting the E-AC3 codec string to 'eac3' when it
  // should be 'ec-3'.  Remove this when datagen is fixed.
  if (util::MimeTypes::IsAudio(mime_type) && codecs == "eac3") {
    fixed_codecs = "ec-3";
  }

  return util::Format(id, mime_type, width, height, frame_rate,
                      max_playout_rate, audio_channels, audio_sampling_rate,
                      bandwidth, language, fixed_codecs);
}

std::unique_ptr<Representation>
MediaPresentationDescriptionParser::BuildRepresentation(
    const std::string& content_id,
    int32_t revision_id,
    util::Format format,
    SegmentBase* segment_base,
    std::vector<std::unique_ptr<DescriptorType>>* supplemental_properties,
    std::vector<std::unique_ptr<DescriptorType>>* essential_properties) const {
  return std::unique_ptr<Representation>(Representation::NewInstance(
      content_id, revision_id, format, segment_base, "",
      supplemental_properties, essential_properties));
}

std::unique_ptr<Representation>
MediaPresentationDescriptionParser::BuildRepresentation(
    const std::string& content_id,
    int32_t revision_id,
    util::Format format,
    std::unique_ptr<SegmentBase> segment_base_override,
    std::vector<std::unique_ptr<DescriptorType>>* supplemental_properties,
    std::vector<std::unique_ptr<DescriptorType>>* essential_properties) const {
  return std::unique_ptr<Representation>(Representation::NewInstance(
      content_id, revision_id, format, std::move(segment_base_override), "",
      supplemental_properties, essential_properties));
}

// SegmentBase, SegmentList and SegmentTemplate parsing.

std::unique_ptr<SingleSegmentBase>
MediaPresentationDescriptionParser::ParseSegmentBase(
    xmlTextReaderPtr reader,
    const std::string& base_url,
    const SingleSegmentBase* parent) const {
  std::unique_ptr<std::string> new_base_url =
      std::unique_ptr<std::string>(new std::string(base_url));

  xmlNodePtr node = xmlTextReaderCurrentNode(reader);
  int64_t timescale;
  int64_t presentation_time_offset;
  if (!ParseLong(node, "timescale", &timescale,
                 parent != nullptr ? parent->GetTimeScale() : 1)) {
    LOG(INFO) << "Failed to parse segment base 'timescale' attribute";
    return nullptr;
  }
  if (!ParseLong(node, "presentationTimeOffset", &presentation_time_offset,
                 parent != nullptr ? parent->GetPresentationTimeOffset() : 0)) {
    LOG(INFO)
        << "Failed to parse segment base 'presentationTimeOffset' attribute";
    return nullptr;
  }

  int64_t index_start = parent != nullptr ? parent->GetIndexStart() : 0;
  int64_t index_length = parent != nullptr ? parent->GetIndexLength() : -1;
  std::string index_range_text = GetAttributeValue(node, "indexRange");
  if (!index_range_text.empty() &&
      !ParseRange(index_range_text, &index_start, &index_length)) {
    LOG(INFO) << "Failed to parse segment base 'range' attribute";
    return nullptr;
  }

  std::unique_ptr<RangedUri> initialization(
      parent != nullptr ? parent->GetInitializationUri() : nullptr);
  int ret;
  int parent_depth = xmlTextReaderDepth(reader);
  int depth;
  do {
    ret = xmlTextReaderRead(reader);
    if (ret != 1) {
      break;
    }
    depth = xmlTextReaderDepth(reader);
    xmlNodePtr child = xmlTextReaderCurrentNode(reader);
    if (NodeNameEquals(child, "Initialization")) {
      // Ownership of base url for ranged uri's goes with the segment base
      initialization = ParseInitialization(reader, new_base_url.get());
      if (initialization.get() == nullptr) {
        return nullptr;
      }
    }
  } while (depth > parent_depth);

  return BuildSingleSegmentBase(
      std::move(initialization), timescale, presentation_time_offset,
      std::move(new_base_url), index_start, index_length);
}

bool MediaPresentationDescriptionParser::ParseRange(
    const std::string& range_str,
    int64_t* index_start,
    int64_t* index_length) const {
  const char* range_text = range_str.c_str();
  char* endptr = nullptr;
  *index_start = strtol(range_text, &endptr, 10);
  if (*index_start < 0 || *endptr != '-') {
    LOG(INFO) << "Invalid index range: invalid start";
    return false;
  }
  if (errno == ERANGE) {
    LOG(INFO) << "Invalid index range: start too large";
    return false;
  }
  range_text = endptr + 1;
  int64_t index_end = strtol(range_text, &endptr, 10);
  if (index_end < 0 || *endptr != '\0') {
    LOG(INFO) << "Invalid index range: invalid end";
    return false;
  }
  if (errno == ERANGE) {
    LOG(INFO) << "Invalid index range: end too large";
    return false;
  }
  if (index_end < *index_start) {
    LOG(INFO) << "Invalid index range: end before start";
    return false;
  }
  *index_length = index_end - *index_start + 1;
  return true;
}

std::unique_ptr<SingleSegmentBase>
MediaPresentationDescriptionParser::BuildSingleSegmentBase(
    std::unique_ptr<RangedUri> initialization,
    int64_t timescale,
    int64_t presentation_time_offset,
    std::unique_ptr<std::string> base_url,
    int64_t index_start,
    int64_t index_length) const {
  return std::unique_ptr<SingleSegmentBase>(new SingleSegmentBase(
      std::move(initialization), timescale, presentation_time_offset,
      std::move(base_url), index_start, index_length));
}

std::unique_ptr<SegmentList>
MediaPresentationDescriptionParser::ParseSegmentList(
    xmlTextReaderPtr reader,
    const std::string& base_url,
    SegmentList* parent) const {
  std::unique_ptr<std::string> new_base_url =
      std::unique_ptr<std::string>(new std::string(base_url));
  xmlNodePtr node = xmlTextReaderCurrentNode(reader);
  int64_t timescale;
  int64_t presentation_time_offset;
  int64_t duration;
  int32_t start_number;
  if (!ParseLong(node, "timescale", &timescale,
                 parent != nullptr ? parent->GetTimeScale() : 1)) {
    LOG(INFO) << "Failed to parse segment list 'timescale' attribute";
    return nullptr;
  }
  if (!ParseLong(node, "presentationTimeOffset", &presentation_time_offset,
                 parent != nullptr ? parent->GetPresentationTimeOffset() : 0)) {
    LOG(INFO)
        << "Failed to parse segment list 'presentationTimeOffset' attribute";
    return nullptr;
  }
  if (!ParseLong(node, "duration", &duration,
                 parent != nullptr ? parent->GetDuration() : -1)) {
    LOG(INFO) << "Failed to parse segment list 'duration' attribute";
    return nullptr;
  }
  if (!ParseInt(node, "startNumber", &start_number,
                parent != nullptr ? parent->GetStartNumber() : 1)) {
    LOG(INFO) << "Failed to parse segment list 'startNumber' attribute";
    return nullptr;
  }

  std::unique_ptr<RangedUri> initialization;
  std::unique_ptr<std::vector<SegmentTimelineElement>> timeline;
  std::unique_ptr<std::vector<RangedUri>> segments;
  int ret;
  int parent_depth = xmlTextReaderDepth(reader);
  int depth;
  do {
    ret = xmlTextReaderRead(reader);
    if (ret != 1) {
      break;
    }
    depth = xmlTextReaderDepth(reader);
    xmlNodePtr child = xmlTextReaderCurrentNode(reader);
    if (NodeNameEquals(child, "Initialization")) {
      initialization = ParseInitialization(reader, new_base_url.get());
      if (initialization.get() == nullptr) {
        return nullptr;
      }
    } else if (NodeNameEquals(child, "SegmentTimeline")) {
      timeline = ParseSegmentTimeline(reader);
      if (timeline.get() == nullptr) {
        return nullptr;
      }
    } else if (NodeNameEquals(child, "SegmentURL")) {
      if (segments.get() == nullptr) {
        segments =
            std::unique_ptr<std::vector<RangedUri>>(new std::vector<RangedUri>);
      }
      std::unique_ptr<RangedUri> media =
          ParseSegmentUrl(reader, new_base_url.get());
      if (media.get() == nullptr) {
        return nullptr;
      }
      segments->push_back(*media);
    }
  } while (depth > parent_depth);

  if (parent != nullptr && initialization.get() == nullptr) {
    // Inherit our parent's initialization.
    initialization = parent->GetInitializationUri();
  }

  // NOTE: If no timeline or segments were created at this level, the newly
  // created SegmentList will inherit the parent's timeline/media segments.
  return BuildSegmentList(std::move(new_base_url), std::move(initialization),
                          timescale, presentation_time_offset, start_number,
                          duration, std::move(timeline), std::move(segments),
                          parent);
}

std::unique_ptr<SegmentList>
MediaPresentationDescriptionParser::BuildSegmentList(
    std::unique_ptr<std::string> base_url,
    std::unique_ptr<RangedUri> initialization,
    uint64_t timescale,
    uint64_t presentation_time_offset,
    int32_t start_number,
    uint64_t duration,
    std::unique_ptr<std::vector<SegmentTimelineElement>> timeline,
    std::unique_ptr<std::vector<RangedUri>> segments,
    SegmentList* parent) const {
  return std::unique_ptr<SegmentList>(
      new SegmentList(std::move(base_url), std::move(initialization), timescale,
                      presentation_time_offset, start_number, duration,
                      std::move(timeline), std::move(segments), parent));
}

std::unique_ptr<SegmentTemplate>
MediaPresentationDescriptionParser::ParseSegmentTemplate(
    xmlTextReaderPtr reader,
    const std::string& base_url,
    SegmentTemplate* parent) const {
  std::unique_ptr<std::string> new_base_url =
      std::unique_ptr<std::string>(new std::string(base_url));
  xmlNodePtr node = xmlTextReaderCurrentNode(reader);

  int64_t timescale;
  int64_t presentation_time_offset;
  int64_t duration;
  int32_t start_number;
  if (!ParseLong(node, "timescale", &timescale,
                 parent != nullptr ? parent->GetTimeScale() : 1)) {
    LOG(INFO) << "Failed to parse segment template 'timescale' attribute";
    return nullptr;
  }
  if (!ParseLong(node, "presentationTimeOffset", &presentation_time_offset,
                 parent != nullptr ? parent->GetPresentationTimeOffset() : 0)) {
    LOG(INFO) << "Failed to parse segment template 'presentationTimeOffset' "
                 "attribute";
    return nullptr;
  }
  if (!ParseLong(node, "duration", &duration,
                 parent != nullptr ? parent->GetDuration() : -1)) {
    LOG(INFO) << "Failed to parse segment template 'duration' attribute";
    return nullptr;
  }
  if (!ParseInt(node, "startNumber", &start_number,
                parent != nullptr ? parent->GetStartNumber() : 1)) {
    LOG(INFO) << "Failed to parse segment template 'startNumber' attribute";
    return nullptr;
  }

  std::unique_ptr<UrlTemplate> media_template = ParseUrlTemplate(
      reader, "media",
      parent != nullptr ? parent->GetMediaTemplate() : nullptr);
  std::unique_ptr<UrlTemplate> initialization_template = ParseUrlTemplate(
      reader, "initialization",
      parent != nullptr ? parent->GetInitializationTemplate() : nullptr);

  std::unique_ptr<RangedUri> initialization;
  std::unique_ptr<std::vector<SegmentTimelineElement>> timeline;

  int ret;
  int parent_depth = xmlTextReaderDepth(reader);
  int depth;
  do {
    ret = xmlTextReaderRead(reader);
    if (ret != 1) {
      break;
    }
    depth = xmlTextReaderDepth(reader);
    xmlNodePtr child = xmlTextReaderCurrentNode(reader);
    if (NodeNameEquals(child, "Initialization")) {
      initialization = ParseInitialization(reader, new_base_url.get());
      if (initialization.get() == nullptr) {
        return nullptr;
      }
    } else if (NodeNameEquals(child, "SegmentTimeline")) {
      timeline = ParseSegmentTimeline(reader);
      if (timeline.get() == nullptr) {
        return nullptr;
      }
    }
  } while (depth > parent_depth);

  if (parent != nullptr && initialization.get() == nullptr) {
    // Inherit our parent's initialization.
    initialization = parent->GetInitializationUri();
  }
  // NOTE: If no timeline was created at this level, the newly
  // created SegmentTemplate will inherit the parent's timeline.

  return BuildSegmentTemplate(
      std::move(new_base_url), std::move(initialization), timescale,
      presentation_time_offset, start_number, duration, std::move(timeline),
      std::move(initialization_template), std::move(media_template), parent);
}

std::unique_ptr<SegmentTemplate>
MediaPresentationDescriptionParser::BuildSegmentTemplate(
    std::unique_ptr<std::string> base_url,
    std::unique_ptr<RangedUri> initialization,
    int64_t timescale,
    int64_t presentation_time_offset,
    int32_t start_number,
    int64_t duration,
    std::unique_ptr<std::vector<SegmentTimelineElement>> timeline,
    std::unique_ptr<UrlTemplate> initialization_template,
    std::unique_ptr<UrlTemplate> media_template,
    SegmentTemplate* parent) const {
  return std::unique_ptr<SegmentTemplate>(new SegmentTemplate(
      std::move(base_url), std::move(initialization), timescale,
      presentation_time_offset, start_number, duration, std::move(timeline),
      std::move(initialization_template), std::move(media_template), parent));
}

std::unique_ptr<std::vector<SegmentTimelineElement>>
MediaPresentationDescriptionParser::ParseSegmentTimeline(
    xmlTextReaderPtr reader) const {
  std::unique_ptr<std::vector<SegmentTimelineElement>> segmentTimeline(
      new std::vector<SegmentTimelineElement>);
  int64_t elapsed_time = 0;
  int ret;
  int parent_depth = xmlTextReaderDepth(reader);
  int depth;
  do {
    ret = xmlTextReaderRead(reader);
    if (ret != 1) {
      break;
    }
    depth = xmlTextReaderDepth(reader);
    xmlNodePtr child = xmlTextReaderCurrentNode(reader);
    if (NodeNameEquals(child, "S")) {
      if (!ParseLong(child, "t", &elapsed_time, elapsed_time)) {
        LOG(INFO) << "Failed to parse segment timeline 't' attribute";
        return nullptr;
      }
      int64_t duration;
      if (!ParseLong(child, "d", &duration)) {
        LOG(INFO) << "Failed to parse segment timeline 'd' attribute";
        return nullptr;
      }
      int32_t r_value;
      if (!ParseInt(child, "r", &r_value, 0)) {
        LOG(INFO) << "Failed to parse segment timeline 'r' attribute";
        return nullptr;
      }
      int count = 1 + r_value;
      for (int i = 0; i < count; i++) {
        segmentTimeline->push_back(
            BuildSegmentTimelineElement(elapsed_time, duration));
        elapsed_time += duration;
      }
    }
  } while (depth > parent_depth);
  return segmentTimeline;
}

SegmentTimelineElement
MediaPresentationDescriptionParser::BuildSegmentTimelineElement(
    int64_t elapsed_time,
    int64_t duration) const {
  return SegmentTimelineElement(elapsed_time, duration);
}

std::unique_ptr<UrlTemplate>
MediaPresentationDescriptionParser::ParseUrlTemplate(
    xmlTextReaderPtr reader,
    const std::string& name,
    std::unique_ptr<UrlTemplate> default_value) const {
  xmlNodePtr node = xmlTextReaderCurrentNode(reader);
  std::string valueString = GetAttributeValue(node, name);
  if (!valueString.empty()) {
    return UrlTemplate::Compile(valueString);
  }
  return default_value;
}

std::unique_ptr<RangedUri>
MediaPresentationDescriptionParser::ParseInitialization(
    xmlTextReaderPtr reader,
    const std::string* base_url) const {
  return ParseRangedUrl(reader, base_url, "sourceURL", "range");
}

std::unique_ptr<RangedUri> MediaPresentationDescriptionParser::ParseSegmentUrl(
    xmlTextReaderPtr reader,
    const std::string* base_url) const {
  return ParseRangedUrl(reader, base_url, "media", "mediaRange");
}

std::unique_ptr<RangedUri> MediaPresentationDescriptionParser::ParseRangedUrl(
    xmlTextReaderPtr reader,
    const std::string* base_url,
    const std::string& url_attribute,
    const std::string& range_attribute) const {
  xmlNodePtr node = xmlTextReaderCurrentNode(reader);
  std::string urlText = GetAttributeValue(node, url_attribute);
  int64_t range_start = 0;
  int64_t range_length = -1;
  std::string range_text = GetAttributeValue(node, range_attribute);
  if (!range_text.empty() &&
      !ParseRange(range_text, &range_start, &range_length)) {
    LOG(INFO) << "Failed to parse 'range' attribute";
    return nullptr;
  }
  return BuildRangedUri(base_url, urlText, range_start, range_length);
}

std::unique_ptr<RangedUri> MediaPresentationDescriptionParser::BuildRangedUri(
    const std::string* base_url,
    const std::string& urlText,
    int64_t rangeStart,
    int64_t rangeLength) const {
  return std::unique_ptr<RangedUri>(
      new RangedUri(base_url, urlText, rangeStart, rangeLength));
}

int32_t MediaPresentationDescriptionParser::ParseAudioChannelConfiguration(
    xmlTextReaderPtr reader) const {
  int32_t audio_channels;
  xmlNodePtr node = xmlTextReaderCurrentNode(reader);
  std::string scheme_id_uri = ParseString(node, "schemeIdUri", "");
  if ("urn:mpeg:dash:23003:3:audio_channel_configuration:2011" ==
      scheme_id_uri) {
    if (!ParseInt(node, "value", &audio_channels, -1)) {
      LOG(WARNING) << "Audio channel configuration has bad value";
    }
  } else {
    audio_channels = -1;
  }
  return audio_channels;
}

// Utility methods.
std::string MediaPresentationDescriptionParser::CheckLanguageConsistency(
    const std::string& firstLanguage,
    const std::string& secondLanguage) const {
  if (firstLanguage.empty()) {
    return secondLanguage;
  } else if (secondLanguage.empty()) {
    return firstLanguage;
  } else {
    DCHECK(firstLanguage == secondLanguage);
    return firstLanguage;
  }
}

AdaptationType MediaPresentationDescriptionParser::CheckContentTypeConsistency(
    AdaptationType firstType,
    AdaptationType secondType) const {
  if (firstType == AdaptationType::UNKNOWN) {
    return secondType;
  } else if (secondType == AdaptationType::UNKNOWN) {
    return firstType;
  } else {
    DCHECK(firstType == secondType);
    return firstType;
  }
}

bool MediaPresentationDescriptionParser::ParseFrameRate(xmlNodePtr node,
                                                        double* out,
                                                        double default_value) {
  double frame_rate = default_value;
  std::string frame_rate_attr = GetAttributeValue(node, "frameRate");
  if (!frame_rate_attr.empty()) {
    std::istringstream f(frame_rate_attr);
    std::string numerator;
    std::string denomniator = "1";
    if (std::getline(f, numerator, '/')) {
      std::getline(f, denomniator, '/');
    }

    // Frame rate numerator and denominator are both integers and base 10.
    int32_t n;
    int32_t d;
    if (!ParseInt(numerator, &n)) {
      LOG(WARNING) << "Could not parse numerator for frame rate";
      return false;
    }
    if (!ParseInt(denomniator, &d)) {
      LOG(WARNING) << "Could not parse denominator for frame rate";
      return false;
    }
    frame_rate = (double)n / (double)d;
  }
  *out = frame_rate;
  return true;
}

int64_t MediaPresentationDescriptionParser::ParseDuration(
    xmlNodePtr node,
    const std::string& name,
    int64_t default_value) {
  xmlChar* value = xmlGetProp(node, TO_XMLCHAR(name.c_str()));
  if (value != nullptr) {
    std::string v(FROM_XMLCHAR(value));
    xmlFree(value);
    return util::Util::ParseXsDuration(v);
  }
  return default_value;
}

std::string MediaPresentationDescriptionParser::GetAttributeValue(
    xmlNodePtr node,
    const std::string& name,
    std::string default_value) {
  xmlChar* value = xmlGetProp(node, TO_XMLCHAR(name.c_str()));
  if (value != nullptr) {
    std::string v(FROM_XMLCHAR(value));
    xmlFree(value);
    return v;
  }
  return default_value;
}

int64_t MediaPresentationDescriptionParser::ParseDateTime(
    xmlNodePtr node,
    const std::string& name,
    int64_t default_value) {
  xmlChar* value = xmlGetProp(node, TO_XMLCHAR(name.c_str()));
  if (value != nullptr) {
    std::string v(FROM_XMLCHAR(value));
    xmlFree(value);
    return util::Util::ParseXsDateTime(v);
  }
  return default_value;
}

std::string MediaPresentationDescriptionParser::ParseBaseUrl(
    xmlTextReaderPtr reader,
    const std::string& parent_base_url) {
  std::string base_url = NextText(reader);
  return util::UriUtil::Resolve(parent_base_url, base_url);
}

bool MediaPresentationDescriptionParser::ParseInt(xmlNodePtr node,
                                                  const std::string& name,
                                                  int32_t* out,
                                                  int32_t default_value) {
  std::string str = GetAttributeValue(node, name);
  *out = default_value;
  if (!str.empty() && !ParseInt(str, out)) {
    return false;
  }
  return true;
}

bool MediaPresentationDescriptionParser::ParseLong(xmlNodePtr node,
                                                   const std::string& name,
                                                   int64_t* out,
                                                   int64_t default_value) {
  std::string str = GetAttributeValue(node, name);
  *out = default_value;
  if (!str.empty() && !ParseLong(str, out)) {
    return false;
  }
  return true;
}

int32_t MediaPresentationDescriptionParser::ParseInt(const std::string& str,
                                                     int32_t* out) {
  int64_t val;
  if (!ParseLong(str, &val)) {
    return false;
  }
  *out = (int32_t)val;
  return true;
}

int64_t MediaPresentationDescriptionParser::ParseLong(const std::string& str,
                                                      int64_t* out) {
  errno = 0;
  *out = strtoll(str.c_str(), nullptr, 10);
  if (errno == ERANGE) {
    LOG(INFO) << "Value out of range: " << str;
    return false;
  } else if (errno == EINVAL) {
    LOG(INFO) << "Invalid value: " << str;
    return false;
  }
  return true;
}

std::string MediaPresentationDescriptionParser::ParseString(
    xmlNodePtr node,
    const std::string& name,
    const std::string& default_value) {
  xmlChar* value = xmlGetProp(node, TO_XMLCHAR(name.c_str()));
  if (value != nullptr) {
    std::string v(FROM_XMLCHAR(value));
    xmlFree(value);
    return v;
  }
  return default_value;
}

bool MediaPresentationDescriptionParser::CurrentNodeNameEquals(
    xmlTextReaderPtr reader,
    const std::string& name) {
  const xmlChar* tag_name = xmlTextReaderConstName(reader);
  if (tag_name != nullptr) {
    std::string tag_name_str(FROM_XMLCHAR(tag_name));
    return name == tag_name_str;
  }
  return false;
}

bool MediaPresentationDescriptionParser::NodeNameEquals(
    xmlNodePtr child,
    const std::string& name) {
  if (child != nullptr && child->name != nullptr) {
    std::string tag_name_str(FROM_XMLCHAR(child->name));
    return name == tag_name_str;
  }
  return false;
}

std::string MediaPresentationDescriptionParser::NextText(
    xmlTextReaderPtr reader) {
  int ret;
  int parent_depth = xmlTextReaderDepth(reader);
  std::string text_value;
  int depth;
  do {
    ret = xmlTextReaderRead(reader);
    if (ret != 1) {
      break;
    }
    depth = xmlTextReaderDepth(reader);
    xmlNodePtr child = xmlTextReaderCurrentNode(reader);
    if (depth == parent_depth + 1 && child->type == XML_TEXT_NODE) {
      xmlChar* v = xmlNodeGetContent(child);
      text_value.append(std::string(FROM_XMLCHAR(v)));
      xmlFree(v);
    }
  } while (depth > parent_depth);

  return text_value;
}

}  // namespace mpd

}  // namespace ndash
