#pragma once
#include "image/Image.h"

namespace rendering
{
// Distance maps currently identify only a static pixel revision, not a frame.
// All request, completion and binding paths must use this same eligibility gate.
inline bool distanceMapEligible(const Image& image, bool enabled)
{
  return enabled && !image.isTimeSeries();
}
} // namespace rendering
