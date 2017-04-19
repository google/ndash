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

#ifndef NDASH_MPD_UNITTEST_HELPER_H_
#define NDASH_MPD_UNITTEST_HELPER_H_

#include <memory>

#include "mpd/adaptation_set.h"
#include "mpd/period.h"
#include "mpd/representation.h"
#include "mpd/segment_template.h"
#include "mpd/single_segment_base.h"
#include "util/format.h"

namespace ndash {

namespace mpd {

std::unique_ptr<char[]> CreateTestSchemeInitData(size_t length);

util::Format CreateTestFormat();

// Create a test SingleSegmentBase
std::unique_ptr<SingleSegmentBase> CreateTestSingleSegmentBase();

// Create a test SegmentTemplate
std::unique_ptr<SegmentTemplate> CreateTestSegmentTemplate(
    int64_t timescale,
    int64_t segment_duration);

// Create a test Representation with a SegmentTemplate
std::unique_ptr<Representation> CreateTestRepresentationWithSegmentTemplate(
    int64_t timescale,
    int64_t segment_duration,
    SegmentBase* segment_base);

// Create a test AdaptationSet of the given type
std::unique_ptr<AdaptationSet> CreateTestAdaptationSet(
    AdaptationType type,
    int64_t timescale,
    int64_t segment_duration);

// Create a test Period with 3 adaptation sets, one for each of video, audio
// and text
std::unique_ptr<Period> CreateTestPeriod(int64_t period_duration,
                                         int64_t timescale,
                                         int64_t segment_duration);

}  // namespace mpd

}  // namespace ndash

#endif  // NDASH_MPD_UNITTEST_HELPER_H_
