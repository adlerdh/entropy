#include "ui/IsosurfaceRangeModel.h"

#include <algorithm>
#include <cmath>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/color_space.hpp>

#include <iomanip>
#include <limits>
#include <sstream>

namespace ui
{
namespace
{

constexpr std::uint32_t k_minRangeCount = 1;
constexpr double k_minSpacing = std::numeric_limits<double>::epsilon();

double rangeDistance(double start, double end)
{
  return std::abs(end - start);
}

} // namespace

double isosurfaceRangeSpacing(double start, double end, std::uint32_t count)
{
  if (count <= k_minRangeCount) {
    return 0.0;
  }

  return rangeDistance(start, end) / static_cast<double>(count - 1);
}

std::uint32_t isosurfaceRangeCountForSpacing(double start, double end, double spacing)
{
  const double distance = rangeDistance(start, end);
  if (distance <= 0.0) {
    return k_minRangeCount;
  }

  if (spacing <= k_minSpacing) {
    return 2;
  }

  const double safeSpacing = std::max(std::abs(spacing), k_minSpacing);
  const double maxIntervalCount = static_cast<double>(std::numeric_limits<std::uint32_t>::max() - 1);
  const double intervalCount = std::clamp(std::round(distance / safeSpacing), 1.0, maxIntervalCount);
  return static_cast<std::uint32_t>(intervalCount) + 1;
}

void updateIsosurfaceRangeSpacing(IsosurfaceRangeParameters& params)
{
  params.count = std::max(params.count, k_minRangeCount);
  params.spacing = isosurfaceRangeSpacing(params.start, params.end, params.count);
}

void updateIsosurfaceRangeCount(IsosurfaceRangeParameters& params)
{
  params.count = isosurfaceRangeCountForSpacing(params.start, params.end, params.spacing);
  updateIsosurfaceRangeSpacing(params);
}

std::vector<double> isosurfaceRangeValues(const IsosurfaceRangeParameters& params)
{
  const std::uint32_t count = std::max(params.count, k_minRangeCount);
  std::vector<double> values;
  values.reserve(count);

  if (count == k_minRangeCount) {
    values.push_back(params.start);
    return values;
  }

  for (std::uint32_t i = 0; i < count; ++i) {
    const double t = static_cast<double>(i) / static_cast<double>(count - 1);
    values.push_back((1.0 - t) * params.start + t * params.end);
  }

  return values;
}

std::string defaultIsosurfaceName(std::size_t index)
{
  std::ostringstream out;
  out << "Surface " << std::setw(2) << std::setfill('0') << index;
  return out.str();
}

glm::vec3 interpolateHsvColor(const glm::vec3& startRgb, const glm::vec3& endRgb, double t)
{
  const double clampedT = std::clamp(t, 0.0, 1.0);

  const glm::vec3 startHsv = glm::hsvColor(startRgb);
  const glm::vec3 endHsv = glm::hsvColor(endRgb);

  float hueDelta = endHsv.x - startHsv.x;
  if (hueDelta > 180.0f) {
    hueDelta -= 360.0f;
  }
  else if (hueDelta < -180.0f) {
    hueDelta += 360.0f;
  }

  glm::vec3 hsv;
  hsv.x = std::fmod(startHsv.x + static_cast<float>(clampedT) * hueDelta + 360.0f, 360.0f);
  hsv.y = (1.0f - static_cast<float>(clampedT)) * startHsv.y + static_cast<float>(clampedT) * endHsv.y;
  hsv.z = (1.0f - static_cast<float>(clampedT)) * startHsv.z + static_cast<float>(clampedT) * endHsv.z;

  return glm::clamp(glm::rgbColor(hsv), glm::vec3{0.0f}, glm::vec3{1.0f});
}

} // namespace ui
