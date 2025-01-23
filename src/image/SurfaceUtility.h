#ifndef SURFACE_UTILITY_H
#define SURFACE_UTILITY_H

#include "image/ImageSettings.h"
#include "image/Isosurface.h"
#include "logic/app/Data.h"

#include <glm/vec4.hpp>

/**
 * @brief Get the color of an isosurface, as a NON-pre-multiplied RGBA 4-vector
 * @param[in] appData Application data
 * @param[in] surface Isosurface
 * @param[in] settings Settings of image associated with isosurface
 * @param[in] comp Image component in question
 * @return Surface color (premultiplied RGBA)
 */
glm::vec4 getIsosurfaceColor(
  const AppData& appData, const Isosurface& surface, const ImageSettings& settings, uint32_t comp
);

#endif // SURFACE_UTILITY_H
