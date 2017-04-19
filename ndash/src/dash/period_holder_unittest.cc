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

#include <cstdint>
#include <limits>
#include <string>

#include <gflags/gflags.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "dash/period_holder.h"
#include "drm/drm_session_manager_mock.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mpd/dash_manifest_representation_parser.h"
#include "test/test_data.h"
#include "track_criteria.h"

namespace ndash {
namespace dash {

using ::testing::Eq;

scoped_refptr<mpd::MediaPresentationDescription> ManifestFromFile(
    const std::string& file) {
  base::FilePath path(FLAGS_test_data_path);
  path = path.AppendASCII(file);

  std::string xml;
  if (!base::ReadFileToString(path, &xml)) {
    return nullptr;
  }

  mpd::MediaPresentationDescriptionParser p;
  scoped_refptr<mpd::MediaPresentationDescription> mpd =
      p.Parse("http://somewhere", base::StringPiece(xml));

  return mpd;
}

TEST(PeriodHolderTest, Accessors) {
  // TODO(adewhurst)
}

TEST(PeriodHolderTest, GetRepresentationIndex) {
  // TODO(adewhurst)
}

TEST(PeriodHolderTest, BuildDrmInitData) {
  // TODO(adewhurst)
}

TEST(PeriodHolderTest, GetPeriodDuration) {
  // TODO(adewhurst)
}

TEST(PeriodHolderTest, CriteriaByConstructorVideoNotTrick) {
  scoped_refptr<mpd::MediaPresentationDescription> manifest =
      ManifestFromFile("mpd/data/trick.xml");
  ASSERT_TRUE(manifest.get() != nullptr);

  drm::MockDrmSessionManager drm_session_manager;
  const TrackCriteria track_criteria("video/*", false /* trick */);
  PeriodHolder period_holder(&drm_session_manager, 0, *manifest, 0,
                             &track_criteria);

  // Should find regular tracks.
  ASSERT_TRUE(period_holder.GetRepresentationHolder("142") != nullptr);
  ASSERT_TRUE(period_holder.GetRepresentationHolder("143") != nullptr);
  ASSERT_TRUE(period_holder.GetRepresentationHolder("144") != nullptr);
  ASSERT_TRUE(period_holder.GetRepresentationHolder("161") != nullptr);
  // No trick tracks.
  ASSERT_TRUE(period_holder.GetRepresentationHolder("401") == nullptr);
  ASSERT_TRUE(period_holder.GetRepresentationHolder("402") == nullptr);
  ASSERT_TRUE(period_holder.GetRepresentationHolder("403") == nullptr);
  ASSERT_TRUE(period_holder.GetRepresentationHolder("404") == nullptr);
}

TEST(PeriodHolderTest, CriteriaByConstructorVideoTrick) {
  scoped_refptr<mpd::MediaPresentationDescription> manifest =
      ManifestFromFile("mpd/data/trick.xml");
  ASSERT_TRUE(manifest.get() != nullptr);

  drm::MockDrmSessionManager drm_session_manager;
  const TrackCriteria track_criteria("video/*", true /* trick */);
  PeriodHolder period_holder(&drm_session_manager, 0, *manifest, 0,
                             &track_criteria);

  // Should find trick tracks.
  ASSERT_TRUE(period_holder.GetRepresentationHolder("401") != nullptr);
  ASSERT_TRUE(period_holder.GetRepresentationHolder("402") != nullptr);
  ASSERT_TRUE(period_holder.GetRepresentationHolder("403") != nullptr);
  ASSERT_TRUE(period_holder.GetRepresentationHolder("404") != nullptr);
  // No regular tracks.
  ASSERT_TRUE(period_holder.GetRepresentationHolder("142") == nullptr);
  ASSERT_TRUE(period_holder.GetRepresentationHolder("143") == nullptr);
  ASSERT_TRUE(period_holder.GetRepresentationHolder("144") == nullptr);
  ASSERT_TRUE(period_holder.GetRepresentationHolder("161") == nullptr);
}

TEST(PeriodHolderTest, CriteriaByConstructorTextLang) {
  scoped_refptr<mpd::MediaPresentationDescription> manifest =
      ManifestFromFile("mpd/data/trick.xml");
  ASSERT_TRUE(manifest.get() != nullptr);

  drm::MockDrmSessionManager drm_session_manager;
  const TrackCriteria track_criteria("text/*", false /* trick */, "es");
  PeriodHolder period_holder(&drm_session_manager, 0, *manifest, 0,
                             &track_criteria);

  // Only 'es' lang.
  ASSERT_TRUE(period_holder.GetRepresentationHolder("es") != nullptr);
  ASSERT_TRUE(period_holder.GetRepresentationHolder("en") == nullptr);
  ASSERT_TRUE(period_holder.GetRepresentationHolder("it") == nullptr);
  ASSERT_TRUE(period_holder.GetRepresentationHolder("fr") == nullptr);
}

TEST(PeriodHolderTest, CriteriaByConstructorNoAudioChannelsPref) {
  scoped_refptr<mpd::MediaPresentationDescription> manifest =
      ManifestFromFile("mpd/data/trick.xml");
  ASSERT_TRUE(manifest.get() != nullptr);

  drm::MockDrmSessionManager drm_session_manager;
  const TrackCriteria track_criteria("audio/*");
  PeriodHolder period_holder(&drm_session_manager, 0, *manifest, 0,
                             &track_criteria);

  // Should see regular 2 chan audio (due to id).
  ASSERT_TRUE(period_holder.GetRepresentationHolder("149") != nullptr);
  ASSERT_TRUE(period_holder.GetRepresentationHolder("150") != nullptr);
  ASSERT_TRUE(period_holder.GetRepresentationHolder("259") == nullptr);
  ASSERT_TRUE(period_holder.GetRepresentationHolder("261") == nullptr);
}

TEST(PeriodHolderTest, CriteriaByConstructorAudioChannelsPref) {
  scoped_refptr<mpd::MediaPresentationDescription> manifest =
      ManifestFromFile("mpd/data/trick.xml");
  ASSERT_TRUE(manifest.get() != nullptr);

  drm::MockDrmSessionManager drm_session_manager;
  const TrackCriteria track_criteria("audio/*", false /* trick */, "", 6);
  PeriodHolder period_holder(&drm_session_manager, 0, *manifest, 0,
                             &track_criteria);

  // Should see 6 channel but not 2.
  ASSERT_TRUE(period_holder.GetRepresentationHolder("259") != nullptr);
  ASSERT_TRUE(period_holder.GetRepresentationHolder("261") != nullptr);
  ASSERT_TRUE(period_holder.GetRepresentationHolder("149") == nullptr);
  ASSERT_TRUE(period_holder.GetRepresentationHolder("150") == nullptr);
}

TEST(PeriodHolderTest, CriteriaByConstructorCodecPref) {
  scoped_refptr<mpd::MediaPresentationDescription> manifest =
      ManifestFromFile("mpd/data/trick.xml");
  ASSERT_TRUE(manifest.get() != nullptr);

  drm::MockDrmSessionManager drm_session_manager;
  const TrackCriteria track_criteria("audio/*", false /* trick */, "", 0,
                                     "mp4a.40.5");
  PeriodHolder period_holder(&drm_session_manager, 0, *manifest, 0,
                             &track_criteria);

  // Should see 6 channel but not 2.
  ASSERT_TRUE(period_holder.GetRepresentationHolder("259") != nullptr);
  ASSERT_TRUE(period_holder.GetRepresentationHolder("261") != nullptr);
  ASSERT_TRUE(period_holder.GetRepresentationHolder("149") == nullptr);
  ASSERT_TRUE(period_holder.GetRepresentationHolder("150") == nullptr);
}

}  // namespace dash
}  // namespace ndash
