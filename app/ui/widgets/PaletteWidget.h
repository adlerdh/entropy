#pragma once

#include "common/PublicTypes.h"

#include <glm/fwd.hpp>

#include <cstddef>
#include <functional>
#include <string>
#include <utility>

class AppData;
class ImageColorMap;
class ImageTransformations;
class LandmarkGroup;
class ParcellationLabelTable;

/**
 * @brief Render a colormap palette selection popup.
 */
void renderPaletteWindow(
  const char* name,
  bool* showPaletteWindow,
  const std::function<std::size_t(void)>& getNumImageColorMaps,
  const std::function<const ImageColorMap*(std::size_t cmapIndex)>& getImageColorMap,
  const std::function<std::size_t(void)>& getCurrentImageColorMapIndex,
  const std::function<void(std::size_t cmapIndex)>& setCurrentImageColormapIndex,
  const std::function<bool(void)>& getImageColorMapInverted,
  const std::function<bool(void)>& getImageColorMapContinuous,
  const std::function<int(void)>& getImageColorMapLevels,
  const glm::vec3& hsvModFactors,
  const std::function<void(void)>& updateImageUniforms);

void renderColorMapWindow();
