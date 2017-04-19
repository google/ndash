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

#ifndef NDASH_DRM_SCHEME_INIT_DATA_H_
#define NDASH_DRM_SCHEME_INIT_DATA_H_

#include <cstdint>
#include <memory>
#include <string>

namespace ndash {

namespace drm {

class SchemeInitData {
 public:
  SchemeInitData(const std::string& mime_type,
                 std::unique_ptr<char[]> data,
                 size_t len);
  SchemeInitData(const SchemeInitData& other);
  ~SchemeInitData();

  bool operator==(const SchemeInitData& other) const;

  const char* GetData() const { return data_.get(); }

  size_t GetLen() const { return len_; }

  const std::string& GetMimeType() const { return mime_type_; }

 private:
  // The mime type of data.
  std::string mime_type_;

  // The initialization data.
  // TODO(rmrossi): Confirm there's no reason for any other class to own
  // this data.
  std::unique_ptr<char[]> data_;
  size_t len_;

  SchemeInitData& operator=(const SchemeInitData& other) = delete;
};

}  // namespace drm

}  // namespace ndash

#endif  // NDASH_DRM_SCHEME_INIT_DATA_H_
