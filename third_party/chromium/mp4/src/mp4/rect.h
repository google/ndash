// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines a simple integer rectangle class.  The containment semantics
// are array-like; that is, the coordinate (x, y) is considered to be
// contained by the rectangle, but the coordinate (x + width, y) is not.
// The class will happily let you create malformed rectangles (that is,
// rectangles with negative width and/or height), but there will be assertions
// in the operations (such as Contains()) to complain in this case.

#ifndef MP4_RECT_H_
#define MP4_RECT_H_

#include <cmath>
#include <iosfwd>
#include <string>

#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"
#include "mp4/point.h"
#include "mp4/size.h"

namespace media {

class MEDIA_EXPORT Rect {
 public:
  Rect() {}
  Rect(int width, int height) : size_(width, height) {}
  Rect(int x, int y, int width, int height)
      : origin_(x, y), size_(width, height) {}
  explicit Rect(const Size& size) : size_(size) {}
  Rect(const Point& origin, const Size& size) : origin_(origin), size_(size) {}

  ~Rect() {}

  int x() const { return origin_.x(); }
  void set_x(int x) { origin_.set_x(x); }

  int y() const { return origin_.y(); }
  void set_y(int y) { origin_.set_y(y); }

  int width() const { return size_.width(); }
  void set_width(int width) { size_.set_width(width); }

  int height() const { return size_.height(); }
  void set_height(int height) { size_.set_height(height); }

  const Point& origin() const { return origin_; }
  void set_origin(const Point& origin) { origin_ = origin; }

  const Size& size() const { return size_; }
  void set_size(const Size& size) { size_ = size; }

  int right() const { return x() + width(); }
  int bottom() const { return y() + height(); }

  Point top_right() const { return Point(right(), y()); }
  Point bottom_left() const { return Point(x(), bottom()); }
  Point bottom_right() const { return Point(right(), bottom()); }

  void SetRect(int x, int y, int width, int height) {
    origin_.SetPoint(x, y);
    size_.SetSize(width, height);
  }

  // Becomes a rectangle that has the same center point but with a size capped
  // at given |size|.
  void ClampToCenteredSize(const Size& size);

  std::string ToString() const;

  bool ApproximatelyEqual(const Rect& rect, int tolerance) const;

 private:
  media::Point origin_;
  media::Size size_;
};

inline bool operator==(const Rect& lhs, const Rect& rhs) {
  return lhs.origin() == rhs.origin() && lhs.size() == rhs.size();
}

inline bool operator!=(const Rect& lhs, const Rect& rhs) {
  return !(lhs == rhs);
}

}  // namespace media

#endif  // MP4_RECT_H_
