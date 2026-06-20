#include "rendering/LocalNccMetric.h"

#include <algorithm>
#include <cmath>

namespace entropy::rendering::local_ncc
{

float metricValue(float ncc, Presentation presentation, bool negativeCorrelationAsMismatch)
{
  ncc = std::clamp(ncc, -1.0f, 1.0f);

  if (Presentation::Correlation == presentation) {
    return 0.5f * (ncc + 1.0f);
  }

  if (negativeCorrelationAsMismatch) {
    return 1.0f - std::clamp(ncc, 0.0f, 1.0f);
  }

  return 0.5f * (1.0f - ncc);
}

int requiredSampleCount(int patchRadius, float minValidFraction)
{
  patchRadius = std::max(patchRadius, 0);
  minValidFraction = std::clamp(minValidFraction, 0.0f, 1.0f);

  const int sideLength = 2 * patchRadius + 1;
  return std::max(1, static_cast<int>(std::ceil(minValidFraction * static_cast<float>(sideLength * sideLength))));
}

} // namespace entropy::rendering::local_ncc
