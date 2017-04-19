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

#include "dash/period_holder.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/strings/pattern.h"
#include "base/time/time.h"
#include "chunk/chunk_extractor_wrapper.h"
#include "dash/exposed_track.h"
#include "dash/representation_holder.h"
#include "drm/drm_init_data.h"
#include "extractor/extractor.h"
#include "extractor/rawcc_parser_extractor.h"
#include "extractor/stream_parser_extractor.h"
#include "mp4/es_descriptor.h"
#include "mp4/media_log.h"
#include "mp4/mp4_stream_parser.h"
#include "mp4/stream_parser.h"
#include "mpd/adaptation_set.h"
#include "mpd/media_presentation_description.h"
#include "mpd/representation.h"
#include "track_criteria.h"
#include "util/mime_types.h"

namespace ndash {
namespace dash {

namespace {

// TODO(rmrossi): Move all this adaptation set logic into a track evaluator
// and give it to dash_chunk_source's constuctor.

bool IsTrick(const mpd::AdaptationSet* set) {
  int n = set->GetSupplementalPropertyCount();
  for (int i = 0; i < n; i++) {
    const std::string& scheme =
        set->GetSupplementalProperty(i)->scheme_id_uri();
    if (scheme == kTrickScheme) {
      return true;
    }
  }
  return false;
}

std::string GetLang(const mpd::AdaptationSet* set) {
  int n = set->NumRepresentations();
  return n > 0 ? set->GetRepresentation(0)->GetFormat().GetLanguage() : "";
}

int32_t GetChannels(const mpd::AdaptationSet* set) {
  int n = set->NumRepresentations();
  return n > 0 ? set->GetRepresentation(0)->GetFormat().GetAudioChannels() : 2;
}

std::string GetCodecs(const mpd::AdaptationSet* set) {
  int n = set->NumRepresentations();
  return n > 0 ? set->GetRepresentation(0)->GetFormat().GetCodecs() : "";
}

// A comparator class that will sort adaptation sets according to their
// attributes (or the attributes of their Representations).  The sort order
// is influenced by the preferred fields from |track_criteria|.
class AdaptationSetComparator {
 public:
  AdaptationSetComparator(const TrackCriteria* track_criteria)
      : track_criteria_(track_criteria) {}

  bool operator()(const mpd::AdaptationSet* set_1,
                  const mpd::AdaptationSet* set_2) {
    // Get the fields we need for sorting order out of each set.
    bool is_trick_1 = IsTrick(set_1);
    bool is_trick_2 = IsTrick(set_2);
    std::string lang_1 = GetLang(set_1);
    std::string lang_2 = GetLang(set_2);
    int32_t channels_1 = GetChannels(set_1);
    int32_t channels_2 = GetChannels(set_2);
    std::string codecs_1 = GetCodecs(set_1);
    std::string codecs_2 = GetCodecs(set_2);

    int32_t trick_val_1 =
        track_criteria_->prefer_trick ? is_trick_1 : !is_trick_1;
    int32_t lang_val_1 = track_criteria_->preferred_lang != ""
                             ? lang_1 == track_criteria_->preferred_lang
                             : 0;
    int32_t channels_val_1 =
        track_criteria_->preferred_channels != 0
            ? (channels_1 >= track_criteria_->preferred_channels ? channels_1
                                                                 : 0)
            : 0;
    int32_t codecs_val_1 = track_criteria_->preferred_codec != ""
                               ? codecs_1 == track_criteria_->preferred_codec
                               : 0;

    int32_t trick_val_2 =
        track_criteria_->prefer_trick ? is_trick_2 : !is_trick_2;
    int32_t lang_val_2 = track_criteria_->preferred_lang != ""
                             ? lang_2 == track_criteria_->preferred_lang
                             : 0;
    int32_t channels_val_2 =
        track_criteria_->preferred_channels != 0
            ? (channels_2 >= track_criteria_->preferred_channels ? channels_2
                                                                 : 0)
            : 0;
    int32_t codecs_val_2 = track_criteria_->preferred_codec != ""
                               ? codecs_2 == track_criteria_->preferred_codec
                               : 0;

    // Order here is arbitrary since not all fields of a track criteria
    // should be applied at the same time.  Typically, only one field is
    // chosen to prefer certain values of that attribute over others.
    return std::tie(trick_val_2, lang_val_2, channels_val_2, codecs_val_2) <
           std::tie(trick_val_1, lang_val_1, channels_val_1, codecs_val_1);
  }

 private:
  const TrackCriteria* track_criteria_;
};
}  // namespace

PeriodHolder::PeriodHolder(drm::DrmSessionManagerInterface* drm_session_manager,
                           int32_t local_index,
                           const mpd::MediaPresentationDescription& manifest,
                           int32_t manifest_index,
                           const TrackCriteria* track_criteria,
                           float playback_rate)
    : local_index_(local_index),
      start_time_(base::TimeDelta::FromMilliseconds(
          manifest.GetPeriod(manifest_index)->GetStartMs())),
      drm_session_manager_(drm_session_manager) {
  const mpd::Period& period = *manifest.GetPeriod(manifest_index);
  base::TimeDelta period_duration = GetPeriodDuration(manifest, manifest_index);
  const mpd::AdaptationSet* adaptation_set =
      SelectAdaptationSet(period, track_criteria);

  if (!adaptation_set) {
    // No adaptation set matches the criteria.  This period will never produce
    // a chunk but we need still need to set proper boundaries.
    UpdateRepresentationIndependentProperties(period_duration, nullptr);
    return;
  }

  drm_init_data_ = BuildDrmInitData(*adaptation_set);

  // We keep track of the representations that exist in the period, to ensure
  // that they don't change during manifest refreshes. A manifest refresh can
  // add/remove segments from a period, but it can't change the representations
  // available.
  for (int32_t index = 0; index < adaptation_set->NumRepresentations();
       index++) {
    representation_indices_.push_back(index);
    const mpd::Representation* representation =
        adaptation_set->GetRepresentation(index);

    std::unique_ptr<chunk::ChunkExtractorWrapper> chunk_extractor;
    if (adaptation_set->GetType() == mpd::AdaptationType::AUDIO ||
        adaptation_set->GetType() == mpd::AdaptationType::VIDEO) {
      std::set<int> audio_object_types;
      if (adaptation_set->GetType() == mpd::AdaptationType::AUDIO) {
        audio_object_types.insert(media::mp4::kISO_14496_3);
        audio_object_types.insert(media::mp4::kAC3);
        audio_object_types.insert(media::mp4::kEAC3);
      }

      std::unique_ptr<media::StreamParser> stream_parser(
          new media::mp4::MP4StreamParser(audio_object_types, false));
      scoped_refptr<media::MediaLog> media_log(new media::MediaLog());
      std::unique_ptr<extractor::ExtractorInterface> extractor(
          new extractor::StreamParserExtractor(drm_session_manager_,
                                               std::move(stream_parser),
                                               media_log, playback_rate < 0));
      chunk_extractor.reset(
          new chunk::ChunkExtractorWrapper(std::move(extractor)));
    } else if (adaptation_set->GetType() == mpd::AdaptationType::TEXT) {
      // For VOD assets with ad insertion, DataGen will send us a closed
      // caption stream (in rawcc format) that represents the entire
      // duration of the show/movie FOR EACH content period. This means we will
      // end up filling our sample queue with data for the whole movie/show even
      // though the period may be only a few minutes. This poses a number of
      // problems for a player.
      //
      //   1) The cc parser would have to skip past a lot of data that occurred
      //      in the past at the beginning of every period until it reached the
      //      current media time (because it will be fed all the data from the
      //      beginning each period).
      //   2) The player would have to ignore/flush data that was pushed to
      //      its sample queue past the period's duration.  Otherwise, the
      //      player will not recognize that the period has ended.
      //
      // To handle this, we will tell our RawCC parser to only push data
      // that falls between the period's start/end times.  That is, give the
      // sample queue what we would have expected from the server in the first
      // place.  The disadvantage of this approach is that we will parse the
      // same data repeatedly.  However, this should not adversely affect
      // performance since the size and complexity of rawcc is small.
      //
      // NOTE: This workaround only applies to application/x-rawcc, un-indexed,
      // single segment representations. Therefore, Sudu LIVE or DVR streams
      // for which rawcc is properly chunked will operate normally and will be
      // unaffected.

      std::unique_ptr<base::TimeDelta> trunc_start;
      std::unique_ptr<base::TimeDelta> trunc_end;
      mpd::SegmentBase* segment_base = representation->GetSegmentBase();
      if (segment_base->IsSingleSegment() &&
          segment_base->GetInitializationUri().get() == nullptr &&
          representation->GetFormat().GetMimeType() ==
              util::kApplicationRAWCC) {
        trunc_start.reset(new base::TimeDelta(base::TimeDelta::FromMicroseconds(
            segment_base->GetPresentationTimeOffsetUs())));
        trunc_end.reset(new base::TimeDelta(*trunc_start + period_duration));

        VLOG(2) << "Truncating single-file un-indexed rawcc stream to between "
                << trunc_start->InSeconds() << " and "
                << trunc_end->InSeconds();
      }

      // There is an assumption here that the player is is using the master
      // timeline to determine when to display CC.
      // TODO(rmrossi): We should provide the sample offset through the API so
      // that the parser can do this adjustment itself.
      base::TimeDelta sample_offset =
          start_time() - base::TimeDelta::FromMicroseconds(
                             representation->GetPresentationTimeOffsetUs());

      std::unique_ptr<extractor::ExtractorInterface> extractor(
          new extractor::RawCCParserExtractor(
              sample_offset, std::move(trunc_start), std::move(trunc_end)));
      chunk_extractor.reset(
          new chunk::ChunkExtractorWrapper(std::move(extractor)));
    }
    representation_holders_.insert(std::make_pair(
        representation->GetFormat().GetId(),
        std::unique_ptr<RepresentationHolder>(new RepresentationHolder(
            start_time_, period_duration, representation,
            std::move(chunk_extractor)))));
  }
  UpdateRepresentationIndependentProperties(
      period_duration,
      adaptation_set->GetRepresentation(*representation_indices_.begin())
          ->GetIndex());
}

PeriodHolder::~PeriodHolder() {}

bool PeriodHolder::UpdatePeriod(
    const mpd::MediaPresentationDescription& manifest,
    int32_t manifest_index,
    const TrackCriteria* track_criteria) {
  const mpd::Period& period = *manifest.GetPeriod(manifest_index);
  base::TimeDelta period_duration = GetPeriodDuration(manifest, manifest_index);
  const mpd::AdaptationSet* adaptation_set =
      SelectAdaptationSet(period, track_criteria);

  if (!adaptation_set) {
    LOG(ERROR) << "No adaptation set found for track criteria!";
    return false;
  }

  for (int32_t index : representation_indices_) {
    const mpd::Representation* representation =
        adaptation_set->GetRepresentation(index);

    if (!representation_holders_.at(representation->GetFormat().GetId())
             ->UpdateRepresentation(period_duration, representation)) {
      return false;  // Re-throw BehindLiveWindowException
    }
  }
  UpdateRepresentationIndependentProperties(
      period_duration,
      adaptation_set->GetRepresentation(*representation_indices_.begin())
          ->GetIndex());

  return true;
}

const mpd::DashSegmentIndexInterface* PeriodHolder::GetArbitrarySegmentIndex()
    const {
  for (const auto& rh : representation_holders_) {
    const mpd::DashSegmentIndexInterface* segment_index =
        rh.second->segment_index();

    if (segment_index)
      return segment_index;
  }

  return nullptr;
}

void PeriodHolder::UpdateRepresentationIndependentProperties(
    base::TimeDelta period_duration,
    const mpd::DashSegmentIndexInterface* segment_index) {
  if (segment_index) {
    int first_segment_num = segment_index->GetFirstSegmentNum();
    int last_segment_num =
        segment_index->GetLastSegmentNum(period_duration.InMicroseconds());
    index_is_unbounded_ =
        last_segment_num == mpd::DashSegmentIndexInterface::kIndexUnbounded;
    index_is_explicit_ = segment_index->IsExplicit();
    available_start_time_ =
        start_time_ + base::TimeDelta::FromMicroseconds(
                          segment_index->GetTimeUs(first_segment_num));
    if (!index_is_unbounded_) {
      available_end_time_ =
          start_time_ +
          base::TimeDelta::FromMicroseconds(
              segment_index->GetTimeUs(last_segment_num)) +
          base::TimeDelta::FromMicroseconds(segment_index->GetDurationUs(
              last_segment_num, period_duration.InMicroseconds()));
    }
  } else {
    index_is_unbounded_ = false;
    index_is_explicit_ = true;
    available_start_time_ = start_time_;
    available_end_time_ = start_time_ + period_duration;
  }
}

bool PeriodHolder::GetRepresentationIndex(
    const mpd::AdaptationSet& adaptation_set,
    const std::string& format_id,
    int32_t* found_index) {
  for (int32_t i = 0; i < adaptation_set.NumRepresentations(); i++) {
    const mpd::Representation* representation =
        adaptation_set.GetRepresentation(i);
    if (format_id == representation->GetFormat().GetId()) {
      *found_index = i;
      return true;
    }
  }
  // This is pretty high severity, but it was originally an
  // IllegalStateException, so this should never actually happen.
  LOG(WARNING) << "Missing format id: " << format_id;
  return false;
}

scoped_refptr<const drm::RefCountedDrmInitData> PeriodHolder::BuildDrmInitData(
    const mpd::AdaptationSet& adaptation_set) {
  scoped_refptr<const drm::RefCountedDrmInitData> drm_init_data;

  if (!adaptation_set.HasContentProtections()) {
    return drm_init_data;  // Null
  }

  drm::MappedDrmInitData* mapped = nullptr;
  const int32_t num_content_protections =
      adaptation_set.NumContentProtections();
  for (int i = 0; i < num_content_protections; i++) {
    const mpd::ContentProtection& content_protection =
        *adaptation_set.GetContentProtection(i);
    const util::Uuid& uuid = content_protection.GetUuid();
    const drm::SchemeInitData* data = content_protection.GetSchemeInitData();

    if (!uuid.is_empty() && data) {
      if (!mapped) {
        mapped = new drm::MappedDrmInitData();
        drm_init_data = mapped;
      }

      // TODO(adewhurst): License request here?

      std::unique_ptr<drm::SchemeInitData> data_copy(
          new drm::SchemeInitData(*data));

      mapped->Put(uuid, std::move(data_copy));
    }
  }

  return drm_init_data;
}

base::TimeDelta PeriodHolder::GetPeriodDuration(
    const mpd::MediaPresentationDescription& manifest,
    int32_t index) {
  int64_t duration_ms = manifest.GetPeriodDuration(index);
  if (duration_ms == -1) {
    return base::TimeDelta();
  } else {
    return base::TimeDelta::FromMilliseconds(duration_ms);
  }
}

// This logic involves filtering/sorting through adaptation sets within a
// period. Currently, this is only called once for 'static' manifests and
// on each manifest refresh for 'dynamic' manifests.
const mpd::AdaptationSet* PeriodHolder::SelectAdaptationSet(
    const mpd::Period& period,
    const TrackCriteria* track_criteria) {
  std::vector<const mpd::AdaptationSet*> all;
  int num_adaptation_sets = period.GetAdaptationSetCount();
  for (int i = 0; i < num_adaptation_sets; i++) {
    const mpd::AdaptationSet* adaptation_set = period.GetAdaptationSet(i);
    // Apply filters. Currently, the only hard filter is the type.  All other
    // criteria are preferred values that affect sorting order.
    if (adaptation_set->NumRepresentations() > 0) {
      const mpd::Representation* representation =
          adaptation_set->GetRepresentation(0);
      std::string rep_mime_type = representation->GetFormat().GetMimeType();
      if (base::MatchPattern(rep_mime_type, track_criteria->mime_type)) {
        all.push_back(adaptation_set);
      }
    }
  }

  if (all.size() == 0) {
    return nullptr;
  }

  // The minimum element according to our comparator will be the one we select.
  AdaptationSetComparator comparator(track_criteria);
  return *std::min_element(std::begin(all), std::end(all), comparator);
}

}  // namespace dash
}  // namespace ndash
