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

#ifndef NDASH_DRM_INIT_DATA_H_
#define NDASH_DRM_INIT_DATA_H_

#include <map>
#include <memory>

#include "base/memory/ref_counted.h"
#include "drm/scheme_init_data.h"
#include "util/uuid.h"

namespace ndash {
namespace drm {

class DrmInitDataInterface {
 public:
  DrmInitDataInterface() {}

  // Get the SchemeInitData associated with the given scheme_uuid.  May return
  // null if no such scheme_uuid is known.
  virtual const SchemeInitData* Get(const util::Uuid& scheme_uuid) const = 0;

 protected:
  virtual ~DrmInitDataInterface();

 private:
  DrmInitDataInterface(const DrmInitDataInterface& other) = delete;
  DrmInitDataInterface& operator=(const DrmInitDataInterface& other) = delete;
};

class RefCountedDrmInitData : public DrmInitDataInterface,
                              public base::RefCounted<RefCountedDrmInitData> {
 public:
  RefCountedDrmInitData() {}

 protected:
  friend class base::RefCounted<RefCountedDrmInitData>;
  ~RefCountedDrmInitData() override;
};

class MappedDrmInitData : public RefCountedDrmInitData {
 public:
  MappedDrmInitData();

  const SchemeInitData* Get(const util::Uuid& scheme_uuid) const override;

  // Associate the SchemeInitData with the given scheme_uuid.
  void Put(const util::Uuid& scheme_uuid,
           std::unique_ptr<SchemeInitData> scheme_init_data);

 protected:
  ~MappedDrmInitData() override;

 private:
  // TODO(adewhurst): If the map gets big, add a hash function for Uuid and
  //                  switch to unordered_map. Perhaps wrap with small_map.
  std::map<util::Uuid, std::unique_ptr<SchemeInitData>> scheme_data_;

  MappedDrmInitData(const MappedDrmInitData& other) = delete;
  MappedDrmInitData& operator=(const MappedDrmInitData& other) = delete;
};

class UniversalDrmInitData : public RefCountedDrmInitData {
 public:
  explicit UniversalDrmInitData(
      std::unique_ptr<SchemeInitData> scheme_init_data);

  const SchemeInitData* Get(const util::Uuid& scheme_uuid) const override;

 protected:
  ~UniversalDrmInitData() override;

 private:
  std::unique_ptr<SchemeInitData> data_;

  UniversalDrmInitData(const UniversalDrmInitData& other) = delete;
  UniversalDrmInitData& operator=(const UniversalDrmInitData& other) = delete;
};

}  // namespace drm
}  // namespace ndash

#endif  // NDASH_DRM_INIT_DATA_H_
