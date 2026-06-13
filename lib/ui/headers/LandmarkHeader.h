#pragma once

#include "common/PublicTypes.h"

#include <uuid.h>

#include <cstddef>
#include <functional>
#include <optional>
#include <string>

class AppData;
struct GuiData;
class Image;
class ImageColorMap;
class ParcellationLabelTable;

/**
 * @brief Render one collapsible landmark-group header for an image.
 */
void renderLandmarkGroupHeader(
  AppData& appData,
  const uuids::uuid& imageUid,
  std::size_t imageIndex,
  bool isActiveImage,
  const AllViewsRecenterType& recenterAllViews);
