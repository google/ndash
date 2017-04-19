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

#include "mpd/mpd_unittest_helper.h"

#include <cstdint>

namespace ndash {

namespace mpd {

std::unique_ptr<char[]> CreateTestSchemeInitData(size_t length) {
  std::unique_ptr<char[]> data(new char[length]());
  for (int i = 0; i < length; i++) {
    data[i] = i;
  }
  return data;
}

util::Format CreateTestFormat() {
  util::Format format("id1", "video/mpeg4", 320, 480, 29.98, 1, 2, 48000,
                      6000000, "en_us", "codec");
  return format;
}

std::unique_ptr<SingleSegmentBase> CreateTestSingleSegmentBase() {
  std::string base_uri = "http://initsource/";
  std::unique_ptr<RangedUri> initialization_uri = std::unique_ptr<RangedUri>(
      new RangedUri(&base_uri, "/init_data", 500, 1000));

  std::unique_ptr<std::string> segment_uri(
      new std::string("http://segmentsource/"));
  std::unique_ptr<SingleSegmentBase> ssb(
      new SingleSegmentBase(std::move(initialization_uri), 1000, 90000,
                            std::move(segment_uri), 0, 30000));
  return ssb;
}

std::unique_ptr<SegmentTemplate> CreateTestSegmentTemplate(
    int64_t timescale,
    int64_t segment_duration) {
  std::unique_ptr<std::string> media_base_uri(new std::string("http://media"));

  std::unique_ptr<UrlTemplate> init_template =
      mpd::UrlTemplate::Compile("http://host/init/$Number$/$Bandwidth$");
  std::unique_ptr<UrlTemplate> media_template =
      mpd::UrlTemplate::Compile("segment/$RepresentationID$/$Number$/");

  std::unique_ptr<SegmentTemplate> segment_template(new SegmentTemplate(
      std::move(media_base_uri), std::unique_ptr<RangedUri>(nullptr), timescale,
      0, 0, segment_duration,
      std::unique_ptr<std::vector<SegmentTimelineElement>>(nullptr),
      std::move(init_template), std::move(media_template)));
  return segment_template;
}

std::unique_ptr<Representation> CreateTestRepresentationWithSegmentTemplate(
    int64_t timescale,
    int64_t segment_duration,
    SegmentBase* segment_base) {
  std::string content_id = "1234";
  int64_t revision_id = 0;
  util::Format format = CreateTestFormat();

  std::unique_ptr<Representation> representation =
      std::unique_ptr<Representation>(Representation::NewInstance(
          content_id, revision_id, format, segment_base));

  return representation;
}

std::unique_ptr<AdaptationSet> CreateTestAdaptationSet(
    AdaptationType type,
    int64_t timescale,
    int64_t segment_duration) {
  std::vector<std::unique_ptr<Representation>> representations;

  std::unique_ptr<SegmentTemplate> segment_template =
      CreateTestSegmentTemplate(timescale, segment_duration);

  representations.push_back(CreateTestRepresentationWithSegmentTemplate(
      timescale, segment_duration, segment_template.get()));

  std::unique_ptr<AdaptationSet> adaptation_set(new AdaptationSet(
      0, type, &representations, nullptr, std::move(segment_template)));

  return adaptation_set;
}

std::unique_ptr<Period> CreateTestPeriod(int64_t start_ms,
                                         int64_t timescale,
                                         int64_t segment_duration) {
  std::unique_ptr<AdaptationSet> video_adaptation = CreateTestAdaptationSet(
      AdaptationType::VIDEO, timescale, segment_duration);
  std::unique_ptr<AdaptationSet> audio_adaptation = CreateTestAdaptationSet(
      AdaptationType::AUDIO, timescale, segment_duration);
  std::unique_ptr<AdaptationSet> text_adaptation = CreateTestAdaptationSet(
      AdaptationType::TEXT, timescale, segment_duration);

  std::vector<std::unique_ptr<AdaptationSet>> adaptation_sets;
  adaptation_sets.push_back(std::move(video_adaptation));
  adaptation_sets.push_back(std::move(audio_adaptation));
  adaptation_sets.push_back(std::move(text_adaptation));

  std::vector<std::unique_ptr<DescriptorType>> supplemental_properties;
  std::unique_ptr<DescriptorType> supplemental(
      new DescriptorType("scheme1", "value1", "id1"));
  supplemental_properties.push_back(std::move(supplemental));

  // Create a period with 3 adaptation sets.
  std::unique_ptr<Period> period(new Period("id", start_ms, &adaptation_sets,
                                            nullptr, &supplemental_properties));

  return period;
}

}  // namespace mpd

}  // namespace ndash
