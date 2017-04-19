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

#include "mpd/content_protections_builder.h"

#include <algorithm>

namespace ndash {
namespace mpd {

namespace {
bool ContentProtectionSorter(const std::unique_ptr<ContentProtection>& c1,
                             const std::unique_ptr<ContentProtection>& c2) {
  return c1->GetSchemeUriId().compare(c2->GetSchemeUriId()) < 0;
}
}  // namespace

ContentProtectionsBuilder::ContentProtectionsBuilder()
    : adaptation_set_protections_(nullptr),
      representation_protections_(nullptr),
      current_representation_protections_(nullptr) {}

ContentProtectionsBuilder::~ContentProtectionsBuilder() {}

bool ContentProtectionsBuilder::AddAdaptationSetProtection(
    std::unique_ptr<ContentProtection> content_protection) {
  if (adaptation_set_protections_.get() == nullptr) {
    adaptation_set_protections_ =
        std::unique_ptr<ContentProtectionList>(new ContentProtectionList);
  }
  return MaybeAddContentProtection(adaptation_set_protections_.get(),
                                   std::move(content_protection));
}

bool ContentProtectionsBuilder::AddRepresentationProtection(
    std::unique_ptr<ContentProtection> content_protection) {
  if (current_representation_protections_ == nullptr) {
    current_representation_protections_ =
        std::unique_ptr<ContentProtectionList>(new ContentProtectionList);
  }
  return MaybeAddContentProtection(current_representation_protections_.get(),
                                   std::move(content_protection));
}

bool ContentProtectionsBuilder::EndRepresentation() {
  bool is_consistent = true;
  if (representation_protections_.get() != nullptr &&
      current_representation_protections_.get() != nullptr) {
    // Assert that each Representation element defines the same
    // ContentProtection elements.
    std::sort(current_representation_protections_->begin(),
              current_representation_protections_->end(),
              ContentProtectionSorter);
    is_consistent =
        std::equal(current_representation_protections_->begin(),
                   current_representation_protections_->end(),
                   representation_protections_->begin(),
                   [](const std::unique_ptr<mpd::ContentProtection>& a,
                      const std::unique_ptr<mpd::ContentProtection>& b) {
                     return *a == *b;
                   });
  } else if (representation_protections_.get() == nullptr &&
             current_representation_protections_.get() != nullptr) {
    std::sort(current_representation_protections_->begin(),
              current_representation_protections_->end(),
              ContentProtectionSorter);
    representation_protections_ =
        std::move(current_representation_protections_);
  } else if (current_representation_protections_.get() == nullptr &&
             representation_protections_.get() != nullptr) {
    is_consistent = false;
  }
  current_representation_protections_.reset();
  return is_consistent;
}

std::unique_ptr<ContentProtectionList> ContentProtectionsBuilder::Build() {
  if (adaptation_set_protections_.get() == nullptr) {
    return std::move(representation_protections_);
  } else if (representation_protections_.get() == nullptr) {
    return std::move(adaptation_set_protections_);
  } else {
    // Bubble up ContentProtection elements found in the child Representation
    // elements.
    bool is_consistent = true;
    for (auto& cp : *representation_protections_) {
      is_consistent &= MaybeAddContentProtection(
          adaptation_set_protections_.get(), std::move(cp));
    }
    if (!is_consistent) {
      return nullptr;
    } else {
      return std::move(adaptation_set_protections_);
    }
  }
}

bool ContentProtectionsBuilder::MaybeAddContentProtection(
    ContentProtectionList* list,
    std::unique_ptr<ContentProtection> content_protection) {
  // TODO(rmrossi): Although these lists are expected to be small, we should
  // consider using a map (sorted by keys) instead of a vector to eliminate
  // linear search like this. Use scheme id as key, if found, use == to
  // do full comparison to determine consistency.
  bool found = false;
  for (auto& cp : *list) {
    if (*cp.get() == *content_protection) {
      found = true;
      break;
    }
  }
  if (!found) {
    for (auto& cp : *list) {
      // If contains returned false (no complete match), but we find a
      // matching schemeUriId, then the MPD contains inconsistent
      // ContentProtection data.
      if (cp->GetSchemeUriId() == content_protection->GetSchemeUriId()) {
        return false;
      }
    }
    list->push_back(std::move(content_protection));
  }
  return true;
}

}  // namespace mpd

}  // namespace ndash
