#include "image/SurfaceUtility.h"

#include <glm/glm.hpp>
#include <uuid.h>
#include <optional>

namespace
{
/// @brief Transform x ∈ [0.0, 1.0] to an integer index i ∈ [0, N-1] with no endpoint bias
template<typename T>
std::size_t mapContinuousToIndex(T x, std::size_t N)
{
  return static_cast<std::size_t>(x * (N - 1) + 0.5); // rounding to nearest
}
}

glm::vec4 getIsosurfaceColor(const AppData& appData, const Isosurface& surface,
                             const ImageSettings& settings, uint32_t comp, bool premult)
{
  if (!settings.applyImageColormapToIsosurfaces()) {
    if (premult) {
      return surface.opacity * glm::vec4{surface.color, 1.0f};
    }
    else {
      return glm::vec4{surface.color, surface.opacity};
    }
  }

  // The colormap is used for the surface color
  const size_t cmapIndex = settings.colorMapIndex(comp);
  const std::optional<uuids::uuid> cmapUid = appData.imageColorMapUid(cmapIndex);

  if (!cmapUid) {
    // Invalid colormap, so return surface color
    return glm::vec4{surface.color, surface.opacity};
  }

  const ImageColorMap* cmap = appData.imageColorMap(*cmapUid);
  if (!cmap) {
    // Invalid colormap, so return surface color
    return glm::vec4{surface.color, surface.opacity};
  }

  // Slope and intercept that map native intensity to normalized [0.0, 1.0] intensity units,
  // where normalized units are based on the window and level settings.
  const auto imgSlopeIntercept = settings.slopeIntercept_normalized_T_native(comp);
  const auto valueNorm = static_cast<float>(imgSlopeIntercept.first * surface.value + imgSlopeIntercept.second);

  // Flip the value if the colormap is inverted and clamp to [0.0, 1.0], in case it is outside the window:
  const float valueNormFlip = settings.isColorMapInverted(comp) ? 1.0f - valueNorm : valueNorm;
  const float valueNormFlipClamp = glm::clamp(valueNormFlip, 0.0f, 1.0f);

  // Index into the colormap:
  const auto colorIndex = mapContinuousToIndex(valueNormFlipClamp, cmap->numColors());
  glm::vec4 cmapColor = cmap->color_RGBA_F32(colorIndex); // Get the pre-multiplied RGBA value

  if (premult) {
    return cmapColor;
  }

  glm::vec4 demultColor{0.0f}; // De-multiply by the alpha component

  if (0.0f != cmapColor.a) {
    demultColor = cmapColor / cmapColor.a;
  }

  demultColor.a *= surface.opacity; // Apply the surface opacity
  return demultColor;
}
