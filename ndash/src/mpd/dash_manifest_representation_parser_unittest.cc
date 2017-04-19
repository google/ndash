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

#include <fstream>
#include <streambuf>
#include <string>
#include <vector>

#include <gflags/gflags.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "gtest/gtest.h"
#include "mpd/dash_manifest_representation_parser.h"
#include "test/test_data.h"
#include "util/util.h"

namespace ndash {

namespace mpd {

TEST(DashParserTests, SegmentBaseTest) {
  MediaPresentationDescriptionParser p;

  std::string xml("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
  xml.append(
      "<MPD type=\"dynamic\" availabilityStartTime=\"2015-04-09T18:46:25\" "
      "mediaPresentationDuration=\"PT3602.585S\" >");
  xml.append(
      "<UTCTiming schemeIdUri=\"urn:mpeg:dash:utc:direct:2012\" "
      "value=\"2016-05-26T11:37:23.586Z\"/>");
  xml.append("<Location>http://mylocation</Location>");
  xml.append("<BaseURL>https://base_mpd</BaseURL>");
  xml.append("<Period start=\"PT663001.024S\">");
  xml.append("<SegmentBase indexRange=\"1000-2000\" indexRangeExact=\"true\">");
  xml.append("<Initialization range=\"0-1000\"/>");
  xml.append("</SegmentBase>");
  xml.append("<AdaptationSet mimeType=\"video/mp4\">");
  xml.append("</AdaptationSet>");
  xml.append("<AdaptationSet mimeType=\"audio/mp4\">");
  xml.append("<BaseURL>https://base_adaptation_set</BaseURL>");
  xml.append(
      "<Representation id=\"141\" codecs=\"mp4a.40.2\" "
      "audioSamplingRate=\"48000\" startWithSAP=\"1\" bandwidth=\"272000\">");
  xml.append("<BaseURL>https://audio_rep_segment_base</BaseURL>");
  xml.append(
      "<SegmentBase indexRange=\"2000-2000\" "
      "indexRangeExact=\"true\">");
  xml.append("<Initialization range=\"100-5000\"/>");
  xml.append("</SegmentBase>");
  xml.append("</Representation>");
  xml.append(
      "<Representation id=\"135\" codecs=\"avc1.64001f\" width=\"854\" "
      "height=\"480\" startWithSAP=\"1\" bandwidth=\"1116000\" "
      "frameRate=\"24000/1001\">");
  xml.append("</Representation>");
  xml.append("</AdaptationSet>");
  xml.append("</Period>");
  xml.append("</MPD>");

  scoped_refptr<MediaPresentationDescription> mpd =
      p.Parse("http://somewhere", base::StringPiece(xml));

  ASSERT_TRUE(mpd.get() != nullptr);
  EXPECT_EQ(true, mpd->IsDynamic());
  EXPECT_EQ(1428605185000LL, mpd->GetAvailabilityStartTime());
  EXPECT_EQ(1, mpd->GetPeriodCount());
  EXPECT_EQ(2, mpd->GetPeriod(0)->GetAdaptationSets().size());

  AdaptationSet* adaptation_set =
      mpd->GetPeriod(0)->GetAdaptationSets().at(1).get();
  ASSERT_TRUE(adaptation_set != nullptr);
  EXPECT_EQ(2, adaptation_set->GetRepresentations()->size());

  const Representation* rep1 =
      adaptation_set->GetRepresentations()->at(0).get();
  const Representation* rep2 =
      adaptation_set->GetRepresentations()->at(1).get();

  ASSERT_TRUE(rep1 != nullptr);
  ASSERT_TRUE(rep2 != nullptr);

  // Test attributes were extracted
  EXPECT_EQ(48000, rep1->GetFormat().GetAudioSamplingRate());
  EXPECT_EQ(272000, rep1->GetFormat().GetBitrate());

  // Initialization range for 1st representation should have been overridden
  // to 100-5000
  EXPECT_EQ(100, rep1->GetInitializationUri()->GetStart());
  EXPECT_EQ(4901, rep1->GetInitializationUri()->GetLength());

  // Initialization range for 2nd representation should have inherited from top
  EXPECT_EQ(0, rep2->GetInitializationUri()->GetStart());
  EXPECT_EQ(1001, rep2->GetInitializationUri()->GetLength());

  EXPECT_NEAR(23.976, rep2->GetFormat().GetFrameRate(), .001);

  // BaseURL for 1st representation should have been overridden
  const RangedUri* index_uri = rep1->GetIndexUri();
  ASSERT_TRUE(index_uri != nullptr);
  EXPECT_EQ("https://audio_rep_segment_base", index_uri->GetUriString());

  // BaseURL for 2nd representation should NOT have been overridden
  index_uri = rep2->GetIndexUri();
  ASSERT_TRUE(index_uri != nullptr);
  EXPECT_EQ("https://base_mpd", index_uri->GetUriString());
}

TEST(DashParserTests, SegmentListTest) {
  MediaPresentationDescriptionParser p;

  std::string xml("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
  xml.append(
      "<MPD type=\"static\" availabilityStartTime=\"2015-04-09T18:46:25\" "
      "mediaPresentationDuration=\"PT3602.585S\" >");
  xml.append(
      "<UTCTiming schemeIdUri=\"urn:mpeg:dash:utc:direct:2012\" "
      "value=\"2016-05-26T11:37:23.586Z\"/>");
  xml.append("<Location>http://mylocation</Location>");
  xml.append("<BaseURL>https://base_mpd</BaseURL>");
  xml.append("<Period start=\"PT663001.024S\">");
  xml.append(
      "<SegmentList presentationTimeOffset=\"50347500\" "
      "startNumber=\"265204\" timescale=\"1000\" duration=\"2500\">");
  xml.append("<Initialization sourceURL=\"https://init_url\"/>");
  xml.append("<SegmentURL media=\"seg0\"/>");
  xml.append("<SegmentURL media=\"seg1\"/>");
  xml.append("<SegmentURL media=\"seg2\"/>");
  xml.append("</SegmentList>");
  xml.append("<AdaptationSet mimeType=\"video/mp4\">");
  xml.append("</AdaptationSet>");
  xml.append("<AdaptationSet mimeType=\"audio/mp4\">");
  xml.append("<BaseURL>https://base_adaptation_set</BaseURL>");
  xml.append(
      "<Representation id=\"141\" codecs=\"mp4a.40.2\" "
      "audioSamplingRate=\"48000\" startWithSAP=\"1\" bandwidth=\"272000\">");
  xml.append("<BaseURL>https://audio_rep_segment_base</BaseURL>");
  xml.append(
      "<SegmentList presentationTimeOffset=\"50347500\" "
      "startNumber=\"265204\" timescale=\"1000\">");
  xml.append("<SegmentURL media=\"override_seg0\"/>");
  xml.append("<SegmentURL media=\"override_seg1\"/>");
  xml.append("<SegmentURL media=\"override_seg2\"/>");
  xml.append("</SegmentList>");
  xml.append("</Representation>");
  xml.append(
      "<Representation id=\"135\" codecs=\"avc1.64001f\" width=\"854\" "
      "height=\"480\" startWithSAP=\"1\" bandwidth=\"1116000\" "
      "frameRate=\"30\">");
  xml.append("</Representation>");
  xml.append("</AdaptationSet>");
  xml.append("</Period>");
  xml.append("</MPD>");

  scoped_refptr<MediaPresentationDescription> mpd =
      p.Parse("http://somewhere", base::StringPiece(xml));

  ASSERT_TRUE(mpd.get() != nullptr);
  EXPECT_TRUE(false == mpd->IsDynamic());
  EXPECT_EQ(1428605185000LL, mpd->GetAvailabilityStartTime());
  EXPECT_EQ(1, mpd->GetPeriodCount());
  EXPECT_EQ(2, mpd->GetPeriod(0)->GetAdaptationSets().size());

  AdaptationSet* adaptation_set =
      mpd->GetPeriod(0)->GetAdaptationSets().at(1).get();
  ASSERT_TRUE(adaptation_set != nullptr);
  EXPECT_EQ(2, adaptation_set->GetRepresentations()->size());

  const Representation* rep1 =
      adaptation_set->GetRepresentations()->at(0).get();
  const Representation* rep2 =
      adaptation_set->GetRepresentations()->at(1).get();

  ASSERT_TRUE(rep1 != nullptr);
  ASSERT_TRUE(rep2 != nullptr);

  // Out of range
  ASSERT_TRUE(rep1->GetIndex()->GetSegmentUrl(0) == nullptr);

  // Duration on rep1 should have inherited parent's duration of 2.5 seconds
  int64_t period_duration_us = 3602.585 * util::kMicrosPerSecond;
  int64_t expected_duration_us = 2.5 * util::kMicrosPerSecond;
  EXPECT_EQ(expected_duration_us,
            rep1->GetIndex()->GetDurationUs(265204, period_duration_us));

  // 1st representation's segment list should be overridden
  EXPECT_EQ("https://audio_rep_segment_base/override_seg0",
            rep1->GetIndex()->GetSegmentUrl(265204)->GetUriString());

  // 2nd representation's segment list should NOT be overridden
  EXPECT_EQ("https://base_mpd/seg0",
            rep2->GetIndex()->GetSegmentUrl(265204)->GetUriString());
}

TEST(DashParserTests, SegmentTemplateTest) {
  MediaPresentationDescriptionParser p;

  std::string xml("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
  xml.append(
      "<MPD type=\"static\" availabilityStartTime=\"2015-04-09T18:46:25\" "
      "mediaPresentationDuration=\"PT3602.585S\" >");
  xml.append(
      "<UTCTiming schemeIdUri=\"urn:mpeg:dash:utc:direct:2012\" "
      "value=\"2016-05-26T11:37:23.586Z\"/>");
  xml.append("<Location>http://mylocation</Location>");
  xml.append("<BaseURL>https://base_mpd</BaseURL>");
  xml.append("<Period start=\"PT663001.024S\">");
  xml.append(
      "<SegmentTemplate presentationTimeOffset=\"50347500\" "
      "startNumber=\"265204\" timescale=\"1000\" duration=\"2500\" "
      "media=\"/media/$RepresentationID$/$Number$\">");
  xml.append("<SegmentTimeline>");
  xml.append("<S d=\"2500\"/>");
  xml.append("<S d=\"2500\"/>");
  xml.append("<S d=\"2500\"/>");
  xml.append("</SegmentTimeline>");
  xml.append("</SegmentTemplate>");
  xml.append("<AdaptationSet mimeType=\"video/mp4\">");
  xml.append("</AdaptationSet>");
  xml.append("<AdaptationSet mimeType=\"audio/mp4\">");
  xml.append("<BaseURL>https://base_adaptation_set</BaseURL>");
  xml.append(
      "<Representation id=\"141\" codecs=\"mp4a.40.2\" "
      "audioSamplingRate=\"48000\" startWithSAP=\"1\" bandwidth=\"272000\">");
  xml.append("        <BaseURL>https://audio_rep_segment_base</BaseURL>");
  xml.append(
      "<SegmentTemplate presentationTimeOffset=\"50347500\" "
      "startNumber=\"265204\" timescale=\"1000\" "
      "media=\"/audio/$RepresentationID$/$Number$\">");
  xml.append("<SegmentTimeline>");
  xml.append("<S d=\"3500\"/>");
  xml.append("<S d=\"3500\"/>");
  xml.append("<S d=\"3500\"/>");
  xml.append("</SegmentTimeline>");
  xml.append("</SegmentTemplate>");
  xml.append("</Representation>");
  xml.append(
      "<Representation id=\"135\" codecs=\"avc1.64001f\" width=\"854\" "
      "height=\"480\" startWithSAP=\"1\" bandwidth=\"1116000\" "
      "frameRate=\"30\">");
  xml.append("</Representation>");
  xml.append("</AdaptationSet>");
  xml.append("</Period>");
  xml.append("</MPD>");

  scoped_refptr<MediaPresentationDescription> mpd =
      p.Parse("http://somewhere", base::StringPiece(xml));

  ASSERT_TRUE(mpd.get() != nullptr);
  EXPECT_TRUE(false == mpd->IsDynamic());
  EXPECT_EQ(1428605185000LL, mpd->GetAvailabilityStartTime());
  EXPECT_EQ(1, mpd->GetPeriodCount());
  EXPECT_EQ(2, mpd->GetPeriod(0)->GetAdaptationSets().size());

  AdaptationSet* adaptation_set =
      mpd->GetPeriod(0)->GetAdaptationSets().at(1).get();
  ASSERT_TRUE(adaptation_set != nullptr);
  EXPECT_EQ(2, adaptation_set->GetRepresentations()->size());

  const Representation* rep1 =
      adaptation_set->GetRepresentations()->at(0).get();
  const Representation* rep2 =
      adaptation_set->GetRepresentations()->at(1).get();

  ASSERT_TRUE(rep1 != nullptr);
  ASSERT_TRUE(rep2 != nullptr);

  // Out of range
  ASSERT_TRUE(rep1->GetIndex()->GetSegmentUrl(0) == nullptr);

  // Duration on rep1 should 3.5 second segments as dictated by timeline
  int64_t period_duration_us = 3602.585 * util::kMicrosPerSecond;
  int64_t expected_duration_us = 3.5 * util::kMicrosPerSecond;
  EXPECT_EQ(expected_duration_us,
            rep1->GetIndex()->GetDurationUs(265204, period_duration_us));

  // Duration on rep2 should have inherited parent's duration of 2.5 seconds
  expected_duration_us = 2.5 * util::kMicrosPerSecond;
  EXPECT_EQ(expected_duration_us,
            rep2->GetIndex()->GetDurationUs(265204, period_duration_us));

  // Out of range
  EXPECT_EQ(-1, rep1->GetIndex()->GetDurationUs(0, period_duration_us));

  // 1st representation's segment list should be overridden
  EXPECT_EQ("https://audio_rep_segment_base/audio/141/265204",
            rep1->GetIndex()->GetSegmentUrl(265204)->GetUriString());

  // 2nd representation's segment list should NOT be overridden
  EXPECT_EQ("https://base_mpd/media/135/265204",
            rep2->GetIndex()->GetSegmentUrl(265204)->GetUriString());
}

TEST(DashParserTests, ContentProtection) {
  MediaPresentationDescriptionParser p;

  std::string xml("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
  xml.append(
      "<MPD type=\"dynamic\" availabilityStartTime=\"2015-04-09T18:46:25\" "
      "mediaPresentationDuration=\"PT3602.585S\" "
      "xmlns:cenc=\"urn:mpeg:cenc:2013\">");
  xml.append(
      "<UTCTiming schemeIdUri=\"urn:mpeg:dash:utc:direct:2012\" "
      "value=\"2016-05-26T11:37:23.586Z\"/>");
  xml.append("<Location>http://mylocation</Location>");
  xml.append("<BaseURL>https://base_mpd</BaseURL>");
  xml.append("<Period start=\"PT663001.024S\">");
  xml.append("<SegmentBase indexRange=\"1000-2000\" indexRangeExact=\"true\">");
  xml.append("<Initialization range=\"0-1000\"/>");
  xml.append("</SegmentBase>");
  xml.append("<AdaptationSet mimeType=\"video/mp4\">");
  xml.append("<ContentProtection ");
  xml.append("schemeIdUri=\"urn:uuid:EDEF8BA9-79D6-4ACE-A3C8-27DCD51D21ED\">");
  xml.append(
      "<cenc:pssh>"
      "AAAATHBzc2gBAAAAEHfv7MCyTQKs4zweUuL7SwAAAAIwMTIzNDU2Nzg5MDEyMzQ1QUJDREVG"
      "R0hJSktMTU5PUAAAAAA=</cenc:pssh>");
  xml.append("</ContentProtection>");
  xml.append("</AdaptationSet>");
  xml.append("</Period>");
  xml.append("</MPD>");

  scoped_refptr<MediaPresentationDescription> mpd =
      p.Parse("http://somewhere", base::StringPiece(xml));

  ASSERT_TRUE(mpd.get() != nullptr);

  AdaptationSet* adaptation_set =
      mpd->GetPeriod(0)->GetAdaptationSets().at(0).get();
  EXPECT_EQ(true, adaptation_set->HasContentProtection());
}

void ParseFromFile(const std::string& file, int num_periods) {
  base::FilePath path(FLAGS_test_data_path);
  path = path.AppendASCII(file);

  std::string xml;
  ASSERT_TRUE(base::ReadFileToString(path, &xml));

  MediaPresentationDescriptionParser p;
  scoped_refptr<MediaPresentationDescription> mpd =
      p.Parse("http://somewhere", base::StringPiece(xml));

  ASSERT_TRUE(mpd.get() != nullptr);
  EXPECT_EQ(num_periods, mpd->GetPeriodCount());
}

TEST(DashParserTests, SuduSegmentListIvod) {
  ParseFromFile("mpd/data/ivod_sl_manifest.xml", 1);
}

TEST(DashParserTests, SuduSegmentListLive) {
  ParseFromFile("mpd/data/live_sl_manifest.xml", 1);
}

TEST(DashParserTests, SuduSegmentTemplatetIvod) {
  ParseFromFile("mpd/data/ivod_st_manifest.xml", 1);
}

TEST(DashParserTests, SuduSegmentTemplateLive) {
  ParseFromFile("mpd/data/live_st_manifest.xml", 1);
}

TEST(DashParserTests, SuduVOD) {
  ParseFromFile("mpd/data/vod_manifest.xml", 1);
}

TEST(DashParserTests, MultiPeriod) {
  ParseFromFile("mpd/data/mp_manifest.xml", 3);
}

TEST(DashParserTests, DataGenBug31863242) {
  MediaPresentationDescriptionParser p;

  std::string xml("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
  xml.append(
      "<MPD type=\"dynamic\" availabilityStartTime=\"2015-04-09T18:46:25\" "
      "mediaPresentationDuration=\"PT3602.585S\" >");
  xml.append(
      "<UTCTiming schemeIdUri=\"urn:mpeg:dash:utc:direct:2012\" "
      "value=\"2016-05-26T11:37:23.586Z\"/>");
  xml.append("<Location>http://mylocation</Location>");
  xml.append("<BaseURL>https://base_mpd</BaseURL>");
  xml.append("<Period start=\"PT663001.024S\">");
  xml.append("<SegmentBase indexRange=\"1000-2000\" indexRangeExact=\"true\">");
  xml.append("<Initialization range=\"0-1000\"/>");
  xml.append("</SegmentBase>");
  xml.append("<AdaptationSet mimeType=\"audio/mp4\">");
  xml.append("<BaseURL>https://base_adaptation_set</BaseURL>");
  // DataGen b/31863242 - eac3 should be changed to ec-3
  xml.append(
      "<Representation id=\"141\" codecs=\"eac3\" "
      "audioSamplingRate=\"48000\" startWithSAP=\"1\" bandwidth=\"272000\">");
  xml.append("<BaseURL>https://audio_rep_segment_base</BaseURL>");
  xml.append(
      "<SegmentBase indexRange=\"2000-2000\" "
      "indexRangeExact=\"true\">");
  xml.append("<Initialization range=\"100-5000\"/>");
  xml.append("</SegmentBase>");
  xml.append("</Representation>");
  xml.append(
      "<Representation id=\"135\" codecs=\"avc1.64001f\" width=\"854\" "
      "height=\"480\" startWithSAP=\"1\" bandwidth=\"1116000\" "
      "frameRate=\"24000/1001\">");
  xml.append("</Representation>");
  xml.append("</AdaptationSet>");
  xml.append("</Period>");
  xml.append("</MPD>");

  scoped_refptr<MediaPresentationDescription> mpd =
      p.Parse("http://somewhere", base::StringPiece(xml));

  ASSERT_TRUE(mpd.get() != nullptr);

  AdaptationSet* adaptation_set =
      mpd->GetPeriod(0)->GetAdaptationSets().at(0).get();
  const Representation* representation = adaptation_set->GetRepresentation(0);
  EXPECT_EQ("ec-3", representation->GetFormat().GetCodecs());
}

}  // namespace mpd

}  // namespace ndash
