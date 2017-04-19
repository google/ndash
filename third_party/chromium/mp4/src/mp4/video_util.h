// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_UTIL_H_
#define MEDIA_BASE_VIDEO_UTIL_H_

#include <stdint.h>

#include "base/memory/ref_counted.h"
#include "mp4/media_export.h"
#include "mp4/rect.h"
#include "mp4/size.h"

namespace media {

// Computes the size of |visible_size| for a given aspect ratio.
MEDIA_EXPORT Size GetNaturalSize(const Size& visible_size,
                                      int aspect_ratio_numerator,
                                      int aspect_ratio_denominator);

// Return the largest centered rectangle with the same aspect ratio of |content|
// that fits entirely inside of |bounds|.  If |content| is empty, its aspect
// ratio would be undefined; and in this case an empty Rect would be returned.
MEDIA_EXPORT Rect ComputeLetterboxRegion(const Rect& bounds,
                                              const Size& content);

// Return a scaled |size| whose area is less than or equal to |target|, where
// one of its dimensions is equal to |target|'s.  The aspect ratio of |size| is
// preserved as closely as possible.  If |size| is empty, the result will be
// empty.
MEDIA_EXPORT Size ScaleSizeToFitWithinTarget(const Size& size,
                                                  const Size& target);

// Return a scaled |size| whose area is greater than or equal to |target|, where
// one of its dimensions is equal to |target|'s.  The aspect ratio of |size| is
// preserved as closely as possible.  If |size| is empty, the result will be
// empty.
MEDIA_EXPORT Size ScaleSizeToEncompassTarget(const Size& size,
                                                  const Size& target);

// Returns |size| with only one of its dimensions increased such that the result
// matches the aspect ratio of |target|.  This is different from
// ScaleSizeToEncompassTarget() in two ways: 1) The goal is to match the aspect
// ratio of |target| rather than that of |size|.  2) Only one of the dimensions
// of |size| may change, and it may only be increased (padded).  If either
// |size| or |target| is empty, the result will be empty.
MEDIA_EXPORT Size PadToMatchAspectRatio(const Size& size,
                                             const Size& target);

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_UTIL_H_
