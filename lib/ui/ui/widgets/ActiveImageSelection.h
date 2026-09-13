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
 * @brief Render a combo box used to choose the active image.
 */
void renderActiveImageSelectionCombo(
  std::size_t numImages,
  const std::function<std::pair<std::string, std::string>(std::size_t index)>& getImageDisplayAndFileName,
  const std::function<std::size_t(void)>& getActiveImageIndex,
  const std::function<void(std::size_t)>& setActiveImageIndex,
  bool showText = true);
