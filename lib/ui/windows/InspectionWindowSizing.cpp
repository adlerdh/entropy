#include "ui/windows/InspectionWindowSizing.h"

#include <algorithm>

namespace ui
{

float fittedInspectionWindowHeight(float desiredHeight, float viewportHeight, float minimumHeight, float maximumHeight)
{
  constexpr float k_maximumViewportFraction = 0.45f;

  const float nonnegativeViewportHeight = std::max(0.0f, viewportHeight);
  const float effectiveMaximum =
    std::max(0.0f, std::min(maximumHeight, nonnegativeViewportHeight * k_maximumViewportFraction));
  const float effectiveMinimum = std::min(std::max(0.0f, minimumHeight), effectiveMaximum);
  return std::clamp(desiredHeight, effectiveMinimum, effectiveMaximum);
}

} // namespace ui
