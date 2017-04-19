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

#ifndef NDASH_UPSTREAM_ALLOCATOR_H_
#define NDASH_UPSTREAM_ALLOCATOR_H_

#include <cstdint>
#include <cstdlib>

namespace ndash {
namespace upstream {

// A source of allocations.
class AllocatorInterface {
 public:
  typedef uint8_t AllocationType;

  AllocatorInterface() {}
  virtual ~AllocatorInterface() {}

  virtual AllocationType* Allocate() = 0;
  virtual void Release(AllocationType* allocation) = 0;
  virtual size_t GetTotalBytesAllocated() const = 0;
  virtual size_t GetIndividualAllocationLength() const = 0;
  virtual void Trim(size_t target_size) = 0;
};

}  // namespace upstream
}  // namespace ndash

#endif  // NDASH_UPSTREAM_ALLOCATOR_H_
