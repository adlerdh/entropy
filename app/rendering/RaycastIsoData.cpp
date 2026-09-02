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
#include <optional>
#include <vector>

void Rendering::updateIsosurfaceDataFor3d(
  AppData& appData,
  const uuids::uuid& imageUid,
  const std::optional<uuids::uuid>& onlyIsosurfaceUid)
{
  constexpr int maxNumIsos = 8;

  auto& isoData = appData.renderData().m_isosurfaceData;
  isoData.numIsos = 0;
  std::fill(std::begin(isoData.values), std::end(isoData.values), 0.0f);
  std::fill(std::begin(isoData.opacities), std::end(isoData.opacities), 0.0f);
  std::fill(std::begin(isoData.rimOpacityStrengths), std::end(isoData.rimOpacityStrengths), 0.0f);
  std::fill(std::begin(isoData.rimEmissionStrengths), std::end(isoData.rimEmissionStrengths), 0.0f);
  std::fill(std::begin(isoData.rimPowers), std::end(isoData.rimPowers), 2.0f);
  std::fill(std::begin(isoData.colors), std::end(isoData.colors), glm::vec3{0.0f});

  const Image* image = appData.image(imageUid);
  if (!image) {
    return;
  }

  const ImageSettings& settings = image->settings();
  const auto& rimLighting = appData.renderData().m_meshSurfaceMaterialSettings;

  if (!settings.globalVisibility() || !settings.visibility()) {
    return;
  }

  const uint32_t activeComp = settings.activeComponent();
  int i = 0;

  for (const auto& surfaceUid : appData.isosurfaceUids(imageUid, activeComp)) {
    if (onlyIsosurfaceUid && surfaceUid != *onlyIsosurfaceUid) {
      continue;
    }

    const Isosurface* surface = m_appData.isosurface(imageUid, activeComp, surfaceUid);
    if (!surface) {
      spdlog::warn("Null isosurface {} for image {}", surfaceUid, imageUid);
      continue;
    }

    if (!surface->visibleIn3d) {
      continue;
    }

    const float opacity = surface->opacity * settings.effectiveIsosurfaceOpacityModulator();
    if (opacity <= 0.0f) {
      continue;
    }

    if (i >= maxNumIsos) {
      break;
    }

    // Map isovalue from native image intensity to texture intensity:
    isoData.values[i] = static_cast<float>(settings.mapNativeIntensityToTexture(surface->value));

    isoData.opacities[i] = opacity;

    isoData.rimOpacityStrengths[i] = rimLighting.rimLightingEnabled ? rimLighting.rimOpacityStrength : 0.0f;
    isoData.rimEmissionStrengths[i] = rimLighting.rimLightingEnabled ? rimLighting.rimEmissionStrength : 0.0f;
    isoData.rimPowers[i] = rimLighting.rimPower;

    if (settings.applyImageColormapToIsosurfaces()) {
      // Color the surface using the current image colormap:
      static constexpr bool premult = false;
      isoData.colors[i] = getIsosurfaceColor(m_appData, *surface, settings, activeComp, premult);
    }
    else {
      // Color the surface using its explicitly defined color:
      isoData.colors[i] = surface->color;
    }

    ++i;
    isoData.numIsos = i;
  }
}
