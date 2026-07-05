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

#include <glm/vec3.hpp>

/**
 * @brief Render one collapsible annotation header for an image.
 */
void renderAnnotationsHeader(
  AppData& appData,
  const uuids::uuid& imageUid,
  std::size_t imageIndex,
  bool isActiveImage,
  bool hasFollowingHeader,
  const std::function<void(const uuids::uuid& viewUid, const glm::vec3& worldFwdDirection)>& setViewCameraDirection,
  const std::function<void()>& paintActiveSegmentationWithActivePolygon,
  const AllViewsRecenterType& recenterAllViews);
