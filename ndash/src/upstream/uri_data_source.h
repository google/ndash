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
#ifndef NDASH_UPSTREAM_URI_DATA_SOURCE_H_
#define NDASH_UPSTREAM_URI_DATA_SOURCE_H_

#include "upstream/data_source.h"

namespace ndash {
namespace upstream {

// A component that provides media data from a URI.
class UriDataSourceInterface : public DataSourceInterface {
 public:
  UriDataSourceInterface() {}
  ~UriDataSourceInterface() override {}

  // When the source is open, returns the URI from which data is being read.
  // If redirection occurred, the URI after redirection is the one returned.
  //
  // The returned string is valid until Close()
  virtual const char* GetUri() const = 0;
};

}  // namespace upstream
}  // namespace ndash

#endif  // NDASH_UPSTREAM_URI_DATA_SOURCE_H_
