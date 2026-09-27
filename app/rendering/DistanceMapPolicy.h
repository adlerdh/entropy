#pragma once
#include "image/Image.h"

namespace rendering
{
/// Determine whether distance-map acceleration is enabled and supported for an image.
/// @param image Image to evaluate; time series are excluded because distance-map identity does not track frames.
/// @param enabled Whether distance-map acceleration is requested.
/// @return True for an enabled, non-time-series image.
/// @details Request, completion, and texture-binding paths must use this same eligibility check.
inline bool distanceMapEligible(const Image& image, bool enabled)
{
  return enabled && !image.isTimeSeries();
}
} // namespace rendering
