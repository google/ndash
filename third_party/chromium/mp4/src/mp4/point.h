// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MP4_POINT_H_
#define MP4_POINT_H_

#include <iosfwd>
#include <string>
#include <tuple>

#include "build/build_config.h"
#include "mp4/media_export.h"

namespace media {

// A point has an x and y coordinate.
class MEDIA_EXPORT Point {
 public:
  Point() : x_(0), y_(0) {}
  Point(int x, int y) : x_(x), y_(y) {}

  ~Point() {}

  int x() const { return x_; }
  int y() const { return y_; }
  void set_x(int x) { x_ = x; }
  void set_y(int y) { y_ = y; }

  void SetPoint(int x, int y) {
    x_ = x;
    y_ = y;
  }

  void Offset(int delta_x, int delta_y) {
    x_ += delta_x;
    y_ += delta_y;
  }

  void SetToMin(const Point& other);
  void SetToMax(const Point& other);

  bool IsOrigin() const { return x_ == 0 && y_ == 0; }

  // Returns a string representation of point.
  std::string ToString() const;

 private:
  int x_;
  int y_;
};

inline bool operator==(const Point& lhs, const Point& rhs) {
  return lhs.x() == rhs.x() && lhs.y() == rhs.y();
}

inline bool operator!=(const Point& lhs, const Point& rhs) {
  return !(lhs == rhs);
}

}  // namespace media

#endif  // MP4_POINT_H_
