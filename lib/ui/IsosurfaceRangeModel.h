#pragma once

#include <cstdint>
#include <glm/vec3.hpp>
#include <string>
#include <vector>

namespace entropy::ui
{

/**
 * @brief Editable parameters for creating a set of isosurfaces over a scalar range.
 */
struct IsosurfaceRangeParameters
{
  double start = 0.0;      //!< First isosurface value.
  double end = 1.0;        //!< Last isosurface value.
  double spacing = 1.0;    //!< Distance between adjacent generated values.
  std::uint32_t count = 2; //!< Number of generated values.
};

/**
 * @brief Return the exact spacing implied by the range endpoints and count.
 *
 * @param start First isosurface value.
 * @param end Last isosurface value.
 * @param count Number of generated values.
 * @return Absolute spacing between adjacent values, or zero when fewer than two values are requested.
 */
double isosurfaceRangeSpacing(double start, double end, std::uint32_t count);

/**
 * @brief Return the nearest valid value count for a requested spacing.
 *
 * @param start First isosurface value.
 * @param end Last isosurface value.
 * @param spacing Requested absolute spacing between adjacent values.
 * @return Number of generated values. The result is always at least one.
 */
std::uint32_t isosurfaceRangeCountForSpacing(double start, double end, double spacing);

/**
 * @brief Update spacing from start, end, and count.
 *
 * @param params Range parameters to update.
 */
void updateIsosurfaceRangeSpacing(IsosurfaceRangeParameters& params);

/**
 * @brief Update count from start, end, and spacing, then normalize the exact spacing.
 *
 * @param params Range parameters to update.
 */
void updateIsosurfaceRangeCount(IsosurfaceRangeParameters& params);

/**
 * @brief Generate inclusive isosurface values from range parameters.
 *
 * @param params Range parameters.
 * @return Isosurface values. The first value is start; when count is greater than one, the last value is end.
 */
std::vector<double> isosurfaceRangeValues(const IsosurfaceRangeParameters& params);

/**
 * @brief Return the default display name for an isosurface.
 *
 * @param index One-based surface index.
 * @return Name with indices below 100 padded to at least two digits.
 */
std::string defaultIsosurfaceName(std::size_t index);

/**
 * @brief Interpolate two RGB colors in HSV space.
 *
 * @param startRgb RGB color used at @p t equal to zero.
 * @param endRgb RGB color used at @p t equal to one.
 * @param t Interpolation factor. Values outside [0, 1] are clamped.
 * @return RGB color obtained by interpolating hue along the shortest path and linearly interpolating saturation/value.
 */
glm::vec3 interpolateHsvColor(const glm::vec3& startRgb, const glm::vec3& endRgb, double t);

} // namespace entropy::ui
