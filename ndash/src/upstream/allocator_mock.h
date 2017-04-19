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

#ifndef NDASH_UPSTREAM_ALLOCATOR_MOCK_H_
#define NDASH_UPSTREAM_ALLOCATOR_MOCK_H_

#include <cstdlib>

#include "gmock/gmock.h"
#include "upstream/allocator.h"

namespace ndash {
namespace upstream {

class MockAllocator : public AllocatorInterface {
 public:
  MockAllocator();
  ~MockAllocator() override;

  MOCK_METHOD0(Allocate, AllocationType*());
  MOCK_METHOD1(Release, void(AllocationType*));
  MOCK_CONST_METHOD0(GetTotalBytesAllocated, size_t());
  MOCK_CONST_METHOD0(GetIndividualAllocationLength, size_t());
  MOCK_METHOD1(Trim, void(size_t));
};

}  // namespace upstream
}  // namespace ndash

#endif  // NDASH_UPSTREAM_ALLOCATOR_MOCK_H_
