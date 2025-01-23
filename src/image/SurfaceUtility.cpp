#include "image/SurfaceUtility.h"

#include <glm/glm.hpp>
#include <optional>
#include <uuid.h>

glm::vec4 getIsosurfaceColor(
  const AppData& appData, const Isosurface& surface, const ImageSettings& settings, uint32_t comp
)
{
  if (settings.applyImageColormapToIsosurfaces())
  {
    // The colormap is used for the surface color
    const size_t cmapIndex = settings.colorMapIndex(comp);

    const std::optional<uuids::uuid> cmapUid = appData.imageColorMapUid(cmapIndex);

    if (!cmapUid)
    {
      // Invalid colormap, so return surface color
      return glm::vec4{surface.color, surface.opacity};
    }

    const ImageColorMap* cmap = appData.imageColorMap(*cmapUid);

    if (!cmap)
    {
      // Invalid colormap, so return surface color
      return glm::vec4{surface.color, surface.opacity};
    }

    // Slope and intercept that map native intensity to normalized [0.0, 1.0] intensity units,
    // where normalized units are based on the window and level settings.
    const std::pair<double, double> imgSlopeIntercept = settings.slopeIntercept_normalized_T_native(
      comp
    );
    const float valueNorm = static_cast<float>(
      imgSlopeIntercept.first * surface.value + imgSlopeIntercept.second
    );

    // Flip the value if the colormap is inverted:
    const float valueNormFlip = settings.isColorMapInverted(comp) ? 1.0f - valueNorm : valueNorm;

    // Clamp value to [0.0, 1.0], in case it is outside the window:
    const float valueNormFlipClamp = glm::clamp(valueNormFlip, 0.0f, 1.0f);

    // Index into the colormap:
    const size_t cmapSample = static_cast<size_t>(
      valueNormFlipClamp * static_cast<float>((cmap->numColors() - 1))
    );

    // Get the premultiplied RGBA value:
    glm::vec4 cmapColor = cmap->color_RGBA_F32(cmapSample);

    // De-multiply by the alpha component:
    glm::vec4 demultColor{0.0f};

    if (0.0f != cmapColor.a)
    {
      demultColor = cmapColor / cmapColor.a;
    }

    // Apply the surface opacity:
    demultColor.a *= surface.opacity;

    return demultColor;
  }
  else
  {
    // Return non-premultiplied color:
    return glm::vec4{surface.color, surface.opacity};
  }
}
