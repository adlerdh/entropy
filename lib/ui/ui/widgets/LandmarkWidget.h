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
 * @brief Render the editable landmarks child window for one landmark group.
 */
void renderLandmarkChildWindow(
  const AppData& appData,
  const ImageTransformations& imageTransformations,
  LandmarkGroup* activeLmGroup,
  const glm::vec3& worldCrosshairsPos,
  const std::function<void(const glm::vec3& worldCrosshairsPos)>& setWorldCrosshairsPos,
  const AllViewsRecenterType& recenterAllViews);
