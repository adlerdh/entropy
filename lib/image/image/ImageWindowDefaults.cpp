#include "image/ImageWindowDefaults.h"
#include "image/ImageUtility.h"

#include "common/Types.h"

#include <cmath>
#include <optional>

namespace image_window_defaults
{

std::optional<WindowRange> rasterColorComponentWindow(ComponentType componentType, const ComponentStats& stats)
{
  if (isUnsignedIntegerType(componentType)) {
    const auto [low, high] = componentRange(componentType);
    if (low < high) {
      return WindowRange{low, high};
    }
  }

  const double low = static_cast<double>(stats.onlineStats.min);
  const double high = static_cast<double>(stats.onlineStats.max);
  if (std::isfinite(low) && std::isfinite(high)) {
    return WindowRange{low, high};
  }

  return std::nullopt;
}

} // namespace image_window_defaults
