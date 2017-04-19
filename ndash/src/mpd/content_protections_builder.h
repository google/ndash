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

#ifndef NDASH_MPD_CONTENT_PROTECTIONS_BUILDER_H_
#define NDASH_MPD_CONTENT_PROTECTIONS_BUILDER_H_

#include "mpd/content_protection.h"

#include <memory>
#include <vector>

namespace ndash {

namespace mpd {

typedef std::vector<std::unique_ptr<ContentProtection>> ContentProtectionList;

// Builds a list of ContentProtection elements for an AdaptationSet.
// If child Representation elements contain ContentProtection elements,
// then it is required that they all define the same ones. If they do, the
// ContentProtection elements are bubbled up to the AdaptationSet. Child
// Representation elements defining different ContentProtection elements is
// considered an error.
class ContentProtectionsBuilder {
 public:
  ContentProtectionsBuilder();
  ~ContentProtectionsBuilder();

  // Adds a ContentProtection found in the AdaptationSet element.
  // Returns false upon consistency check failure.
  bool AddAdaptationSetProtection(
      std::unique_ptr<ContentProtection> content_protection);

  // Adds a ContentProtection found in a child Representation element.
  // Returns false upon consistency check failure.
  bool AddRepresentationProtection(
      std::unique_ptr<ContentProtection> content_protection);

  // Should be invoked after processing each child Representation element,
  // in order to apply consistency checks. Returns false upon consistency
  // failure, true otherwise.
  bool EndRepresentation();

  // Returns the final list of consistent ContentProtection elements. If in
  // attempting to compile the final list an inconsistency is detected, this
  // method will return null.
  std::unique_ptr<ContentProtectionList> Build();

 private:
  std::unique_ptr<ContentProtectionList> adaptation_set_protections_;
  std::unique_ptr<ContentProtectionList> representation_protections_;
  std::unique_ptr<ContentProtectionList> current_representation_protections_;

  // Checks a ContentProtection for consistency with the given list,
  // adding it if necessary.
  // - If the new ContentProtection matches another in the list, it's
  // consistent and is not added to the list returning true.
  // - If the new ContentProtection has the same schemeUriId as another
  // ContentProtection in the list, but its other attributes do not match,
  // then it's inconsistent false is returned.
  // - Else the new ContentProtection has a unique schemeUriId, it's
  // consistent and is added, returning true.
  bool MaybeAddContentProtection(
      ContentProtectionList* list,
      std::unique_ptr<ContentProtection> content_protection);
};

}  // namespace mpd

}  // namespace ndash

#endif  // NDASH_MPD_CONTENT_PROTECTIONS_BUILDER_H_
