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

#ifndef NDASH_UPSTREAM_DEFAULT_ALLOCATOR_H_
#define NDASH_UPSTREAM_DEFAULT_ALLOCATOR_H_

#include <cstdint>
#include <cstdlib>

#include "upstream/allocator.h"

namespace ndash {
namespace upstream {

class DefaultAllocator : public AllocatorInterface {
 public:
  // Constructs an initially empty pool.
  DefaultAllocator(size_t individual_allocation_size);
  // Constructs a pool with some Allocations created up front.
  // Note: the initial allocations aren't actually done currently.
  DefaultAllocator(size_t individual_allocation_size,
                   size_t initial_allocation_count);
  ~DefaultAllocator() override;

  AllocationType* Allocate() override;
  void Release(AllocationType* allocation) override;
  size_t GetTotalBytesAllocated() const override;
  size_t GetIndividualAllocationLength() const override;
  void Trim(size_t target_size) override;

 private:
  const size_t individual_allocation_size_;
  size_t allocated_count_ = 0;
};

}  // namespace upstream
}  // namespace ndash

#endif  // NDASH_UPSTREAM_DEFAULT_ALLOCATOR_H_
