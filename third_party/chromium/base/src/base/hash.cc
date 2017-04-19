// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/hash.h"

#include <functional>

namespace base {

uint32_t SuperFastHash(const char* data, size_t len) {
  std::hash<std::string> hash_fn;
  return hash_fn(std::string(data, len));
}

}  // namespace base
