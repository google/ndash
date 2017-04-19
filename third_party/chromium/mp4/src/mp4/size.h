// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MP4_SIZE_H_
#define MP4_SIZE_H_

#include <iosfwd>
#include <string>

#include "base/compiler_specific.h"
#include "base/numerics/safe_math.h"
#include "build/build_config.h"
#include "mp4/media_export.h"

namespace media {

// A size has width and height values.
class MEDIA_EXPORT Size {
 public:
  Size() : width_(0), height_(0) {}
  Size(int width, int height)
      : width_(width < 0 ? 0 : width), height_(height < 0 ? 0 : height) {}

  ~Size() {}

  int width() const { return width_; }
  int height() const { return height_; }

  void set_width(int width) { width_ = width < 0 ? 0 : width; }
  void set_height(int height) { height_ = height < 0 ? 0 : height; }

  void SetSize(int width, int height) {
    set_width(width);
    set_height(height);
  }

  bool IsEmpty() const { return !width() || !height(); }

  std::string ToString() const;

 private:
  int width_;
  int height_;
};

inline bool operator==(const Size& lhs, const Size& rhs) {
  return lhs.width() == rhs.width() && lhs.height() == rhs.height();
}

inline bool operator!=(const Size& lhs, const Size& rhs) {
  return !(lhs == rhs);
}

}  // namespace media

#endif  // MP4_SIZE_H_
