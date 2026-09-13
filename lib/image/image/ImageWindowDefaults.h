#pragma once

#include "common/Types.h"

#include <optional>

/**
 * @brief Window/level defaults that depend on image source and pixel interpretation.
 */
namespace image_window_defaults
{

/**
 * @brief Native intensity range used for an initial component window.
 */
struct WindowRange
{
  double low = 0.0;  //!< Low native component value
  double high = 0.0; //!< High native component value
};

/**
 * @brief Return the default display window for one RGB/RGBA raster component.
 *
 * Standard raster color images are usually display data, not scalar medical intensities. Unsigned
 * integer channels should therefore open with their full channel range so colors are not changed by
 * quantile clipping. Signed integer and floating-point color data do not have a useful full type
 * range for display, so their measured finite data range is the safer default.
 */
std::optional<WindowRange> rasterColorComponentWindow(ComponentType componentType, const ComponentStats& stats);

} // namespace image_window_defaults
