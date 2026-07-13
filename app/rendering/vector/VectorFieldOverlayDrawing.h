#pragma once

#include "common/DirectionMaps.h"

#include <glm/fwd.hpp>

class Image;
class ImageSettings;
class View;
class Viewport;

namespace rendering::vector_overlay
{

/**
 * @brief Transform a view-space direction into an image's subject coordinate frame.
 */
glm::vec3 subjectViewDirection(const Image& image, const View& view, Directions::View direction);

/**
 * @brief Estimate screen pixels per millimeter at a world-space point in a view.
 */
float screenPixelsPerMillimeter(const Viewport& windowViewport, const View& view, const glm::vec3& worldCrosshairs);

/**
 * @brief Estimate screen pixels per image voxel at a world-space point in a view.
 */
float screenPixelsPerVoxel(
  const Viewport& windowViewport,
  const View& view,
  const Image& image,
  const glm::vec3& worldCrosshairs);

/**
 * @brief Convert vector warped-grid spacing settings to millimeters in image subject space.
 */
float warpedGridSpacingMillimeters(
  const ImageSettings& settings,
  const Viewport& windowViewport,
  const View& view,
  const Image& image,
  const glm::vec3& worldCrosshairs);

} // namespace rendering::vector_overlay
