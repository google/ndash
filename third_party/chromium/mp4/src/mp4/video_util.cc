// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mp4/video_util.h"

#include <cmath>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/numerics/safe_math.h"

namespace media {

Size GetNaturalSize(const Size& visible_size,
                         int aspect_ratio_numerator,
                         int aspect_ratio_denominator) {
  if (aspect_ratio_denominator == 0 ||
      aspect_ratio_numerator < 0 ||
      aspect_ratio_denominator < 0)
    return Size();

  double aspect_ratio = aspect_ratio_numerator /
      static_cast<double>(aspect_ratio_denominator);

  return Size(round(visible_size.width() * aspect_ratio),
                   visible_size.height());
}

// Helper function to return |a| divided by |b|, rounded to the nearest integer.
static int RoundedDivision(int64_t a, int b) {
  DCHECK_GE(a, 0);
  DCHECK_GT(b, 0);
  base::CheckedNumeric<uint64_t> result(a);
  result += b / 2;
  result /= b;
  return base::checked_cast<int>(result.ValueOrDie());
}

// Common logic for the letterboxing and scale-within/scale-encompassing
// functions.  Scales |size| to either fit within or encompass |target|,
// depending on whether |fit_within_target| is true.
static Size ScaleSizeToTarget(const Size& size,
                                   const Size& target,
                                   bool fit_within_target) {
  if (size.IsEmpty())
    return Size();  // Corner case: Aspect ratio is undefined.

  const int64_t x = static_cast<int64_t>(size.width()) * target.height();
  const int64_t y = static_cast<int64_t>(size.height()) * target.width();
  const bool use_target_width = fit_within_target ? (y < x) : (x < y);
  return use_target_width ?
      Size(target.width(), RoundedDivision(y, size.width())) :
      Size(RoundedDivision(x, size.height()), target.height());
}

Rect ComputeLetterboxRegion(const Rect& bounds,
                                 const Size& content) {
  // If |content| has an undefined aspect ratio, let's not try to divide by
  // zero.
  if (content.IsEmpty())
    return Rect();

  Rect result = bounds;
  result.ClampToCenteredSize(ScaleSizeToTarget(content, bounds.size(), true));
  return result;
}

Size ScaleSizeToFitWithinTarget(const Size& size,
                                     const Size& target) {
  return ScaleSizeToTarget(size, target, true);
}

Size ScaleSizeToEncompassTarget(const Size& size,
                                     const Size& target) {
  return ScaleSizeToTarget(size, target, false);
}

Size PadToMatchAspectRatio(const Size& size,
                                const Size& target) {
  if (target.IsEmpty())
    return Size();  // Aspect ratio is undefined.

  const int64_t x = static_cast<int64_t>(size.width()) * target.height();
  const int64_t y = static_cast<int64_t>(size.height()) * target.width();
  if (x < y)
    return Size(RoundedDivision(y, target.height()), size.height());
  return Size(size.width(), RoundedDivision(x, target.width()));
}

}  // namespace media
