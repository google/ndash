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

#include "drm/drm_init_data.h"

#include <utility>

#include "base/logging.h"
#include "drm/scheme_init_data.h"
#include "util/uuid.h"

namespace ndash {
namespace drm {

DrmInitDataInterface::~DrmInitDataInterface() {}
RefCountedDrmInitData::~RefCountedDrmInitData() {}

MappedDrmInitData::MappedDrmInitData() : scheme_data_() {}
MappedDrmInitData::~MappedDrmInitData() {}

const SchemeInitData* MappedDrmInitData::Get(
    const util::Uuid& scheme_uuid) const {
  auto result = scheme_data_.find(scheme_uuid);

  if (result == scheme_data_.end())
    return nullptr;

  return result->second.get();
}

void MappedDrmInitData::Put(const util::Uuid& scheme_uuid,
                            std::unique_ptr<SchemeInitData> scheme_init_data) {
  scheme_data_[scheme_uuid] = std::move(scheme_init_data);
}

UniversalDrmInitData::UniversalDrmInitData(
    std::unique_ptr<SchemeInitData> scheme_init_data)
    : data_(std::move(scheme_init_data)) {}
UniversalDrmInitData::~UniversalDrmInitData() {}

const SchemeInitData* UniversalDrmInitData::Get(
    const util::Uuid& scheme_uuid) const {
  return data_.get();
}

}  // namespace drm
}  // namespace ndash
