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

#ifndef NDASH_UPSTREAM_HTTP_DATA_SOURCE_MOCK_H_
#define NDASH_UPSTREAM_HTTP_DATA_SOURCE_MOCK_H_

#include <cstdlib>

#include "base/synchronization/cancellation_flag.h"
#include "gmock/gmock.h"
#include "upstream/data_spec.h"
#include "upstream/http_data_source.h"

namespace ndash {
namespace upstream {

class MockHttpDataSource : public HttpDataSourceInterface {
 public:
  MockHttpDataSource();
  ~MockHttpDataSource() override;

  // DataSourceInterface
  MOCK_METHOD2(Open,
               ssize_t(const DataSpec& data_spec,
                       const base::CancellationFlag* cancel));
  MOCK_METHOD0(Close, void());
  MOCK_METHOD2(Read, ssize_t(void* buffer, size_t read_length));

  // UriDataSourceInterface
  MOCK_CONST_METHOD0(GetUri, const char*());

  // HttpDataSourceInterface
  MOCK_METHOD2(SetRequestProperty,
               void(const std::string& name, const std::string& value));
  MOCK_METHOD1(ClearRequestProperty, void(const std::string& name));
  MOCK_METHOD0(ClearAllRequestProperties, void());
  MOCK_CONST_METHOD0(GetResponseHeaders,
                     const std::multimap<std::string, std::string>*());
  MOCK_CONST_METHOD0(GetResponseCode, int());
  MOCK_CONST_METHOD0(GetHttpError, HttpDataSourceError());
  MOCK_METHOD1(ReadAllToString, std::string(size_t max_length));
};

}  // namespace upstream
}  // namespace ndash

#endif  // NDASH_UPSTREAM_HTTP_DATA_SOURCE_MOCK_H_
