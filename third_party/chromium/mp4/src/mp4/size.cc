// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mp4/size.h"

#include "base/numerics/safe_math.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"

namespace media {

std::string Size::ToString() const {
  return base::StringPrintf("%dx%d", width(), height());
}

}  // namespace media
