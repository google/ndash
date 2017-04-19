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

#ifndef NDASH_UPSTREAM_DATA_SOURCE_MOCK_H_
#define NDASH_UPSTREAM_DATA_SOURCE_MOCK_H_

#include <cstdlib>

#include "base/synchronization/cancellation_flag.h"
#include "gmock/gmock.h"
#include "upstream/data_source.h"
#include "upstream/data_spec.h"

namespace ndash {
namespace upstream {

class MockDataSource : public DataSourceInterface {
 public:
  MockDataSource();
  ~MockDataSource() override;

  MOCK_METHOD2(Open, ssize_t(const DataSpec&, const base::CancellationFlag*));
  MOCK_METHOD0(Close, void());
  MOCK_METHOD2(Read, ssize_t(void*, size_t));
};

}  // namespace upstream
}  // namespace ndash

#endif  // NDASH_UPSTREAM_DATA_SOURCE_MOCK_H_
