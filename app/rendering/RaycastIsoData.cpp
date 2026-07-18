#include "rendering/Rendering.h"

#include "image/Image.h"
#include "image/ImageSettings.h"
#include "image/Isosurface.h"
#include "logic/app/Data.h"
#include "logic/SurfaceUtility.h"
#include "rendering/RenderData.h"

#include <glm/glm.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <vector>

namespace
{
const glm::vec3 WHITE{1.0f};
}

void Rendering::updateIsosurfaceDataFor3d(AppData& appData, const uuids::uuid& imageUid)
{
  constexpr int maxNumIsos = 8;

  auto& isoData = appData.renderData().m_isosurfaceData;
  isoData.numIsos = 0;
  std::fill(std::begin(isoData.values), std::end(isoData.values), 0.0f);
  std::fill(std::begin(isoData.opacities), std::end(isoData.opacities), 0.0f);
  std::fill(std::begin(isoData.rimOpacityStrengths), std::end(isoData.rimOpacityStrengths), 0.0f);
  std::fill(std::begin(isoData.rimEmissionStrengths), std::end(isoData.rimEmissionStrengths), 0.0f);
  std::fill(std::begin(isoData.rimPowers), std::end(isoData.rimPowers), 2.0f);
  std::fill(std::begin(isoData.ambient), std::end(isoData.ambient), glm::vec3{0.0f});
  std::fill(std::begin(isoData.diffuse), std::end(isoData.diffuse), glm::vec3{0.0f});
  std::fill(std::begin(isoData.specular), std::end(isoData.specular), glm::vec3{0.0f});
  std::fill(std::begin(isoData.shininesses), std::end(isoData.shininesses), 0.0f);

  const Image* image = appData.image(imageUid);
  if (!image) {
    return;
  }

  const ImageSettings& settings = image->settings();

  if (!settings.globalVisibility() || !settings.visibility() || !settings.isosurfacesVisible()) {
    return;
  }

  const uint32_t activeComp = settings.activeComponent();
  int i = 0;

  for (const auto& surfaceUid : appData.isosurfaceUids(imageUid, activeComp)) {
    const Isosurface* surface = m_appData.isosurface(imageUid, activeComp, surfaceUid);
    if (!surface) {
      spdlog::warn("Null isosurface {} for image {}", surfaceUid, imageUid);
      continue;
    }

    if (!surface->visible) {
      continue;
    }

    const float opacity = surface->opacity * settings.isosurfaceOpacityModulator();
    if (opacity <= 0.0f) {
      continue;
    }

    // Map isovalue from native image intensity to texture intensity:
    isoData.values[i] = static_cast<float>(settings.mapNativeIntensityToTexture(surface->value));

    isoData.opacities[i] = opacity;

    isoData.rimOpacityStrengths[i] = surface->rimLightingEnabled ? surface->rimOpacityStrength : 0.0f;
    isoData.rimEmissionStrengths[i] = surface->rimLightingEnabled ? surface->rimEmissionStrength : 0.0f;
    isoData.rimPowers[i] = surface->rimPower;
    isoData.shininesses[i] = surface->material.shininess;

    if (settings.applyImageColormapToIsosurfaces()) {
      // Color the surface using the current image colormap:
      static constexpr bool premult = false;
      const glm::vec3 cmapColor = getIsosurfaceColor(m_appData, *surface, settings, activeComp, premult);
      isoData.ambient[i] = surface->material.ambient * cmapColor;
      isoData.diffuse[i] = surface->material.diffuse * cmapColor;
      isoData.specular[i] = surface->material.specular * WHITE;
    }
    else {
      // Color the surface using its explicitly defined color:
      isoData.ambient[i] = surface->ambientColor();
      isoData.diffuse[i] = surface->diffuseColor();
      isoData.specular[i] = surface->specularColor();
    }

    ++i;
    isoData.numIsos = i;

    if (i >= maxNumIsos) {
      break;
    }
  }
}
