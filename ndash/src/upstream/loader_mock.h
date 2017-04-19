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

#ifndef NDASH_UPSTREAM_LOADER_MOCK_H_
#define NDASH_UPSTREAM_LOADER_MOCK_H_

#include "gmock/gmock.h"
#include "upstream/loader.h"

namespace ndash {
namespace upstream {

class MockLoadableInterface : public LoadableInterface {
 public:
  MockLoadableInterface();
  ~MockLoadableInterface() override;

  MOCK_METHOD0(CancelLoad, void());
  MOCK_CONST_METHOD0(IsLoadCanceled, bool());
  MOCK_METHOD0(Load, bool());
};

class MockLoaderInterface : public LoaderInterface {
 public:
  MockLoaderInterface();
  ~MockLoaderInterface() override;

  MOCK_METHOD2(StartLoading, bool(LoadableInterface*, const LoadDoneCallback&));
  MOCK_CONST_METHOD0(IsLoading, bool());
  MOCK_METHOD0(CancelLoading, void());
};

}  // namespace upstream
}  // namespace ndash

#endif  // NDASH_UPSTREAM_LOADER_MOCK_H_
