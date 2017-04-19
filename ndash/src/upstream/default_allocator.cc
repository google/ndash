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

#include "upstream/default_allocator.h"

#include "base/logging.h"

namespace ndash {
namespace upstream {

DefaultAllocator::DefaultAllocator(size_t individual_allocation_size)
    : individual_allocation_size_(individual_allocation_size) {
  CHECK_GT(individual_allocation_size, 0);
}

DefaultAllocator::DefaultAllocator(size_t individual_allocation_size,
                                   size_t initial_allocation_count)
    : DefaultAllocator(individual_allocation_size) {
  // TODO(adewhurst): Actually do something special here
}

DefaultAllocator::~DefaultAllocator() {
}

uint8_t* DefaultAllocator::Allocate() {
  allocated_count_++;
  return new AllocationType[individual_allocation_size_];
}

void DefaultAllocator::Release(AllocationType* allocation) {
  allocated_count_--;
  DCHECK(allocation != nullptr);
  delete allocation;
}

size_t DefaultAllocator::GetTotalBytesAllocated() const {
  return allocated_count_ * individual_allocation_size_ *
         sizeof(AllocationType);
}

size_t DefaultAllocator::GetIndividualAllocationLength() const {
  return individual_allocation_size_;
}

// TODO(rmrossi): Implement Trim() if required.
void DefaultAllocator::Trim(size_t target_size) {
  // Unimplemented
}

}  // namespace upstream
}  // namespace ndash
