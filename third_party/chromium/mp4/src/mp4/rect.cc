// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mp4/rect.h"

#include <algorithm>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"

namespace media {

void Rect::ClampToCenteredSize(const Size& size) {
  int new_width = std::min(width(), size.width());
  int new_height = std::min(height(), size.height());
  int new_x = x() + (width() - new_width) / 2;
  int new_y = y() + (height() - new_height) / 2;
  SetRect(new_x, new_y, new_width, new_height);
}

std::string Rect::ToString() const {
  return base::StringPrintf("%s %s",
                            origin().ToString().c_str(),
                            size().ToString().c_str());
}

bool Rect::ApproximatelyEqual(const Rect& rect, int tolerance) const {
  return std::abs(x() - rect.x()) <= tolerance &&
         std::abs(y() - rect.y()) <= tolerance &&
         std::abs(right() - rect.right()) <= tolerance &&
         std::abs(bottom() - rect.bottom()) <= tolerance;
}

}  // namespace media
