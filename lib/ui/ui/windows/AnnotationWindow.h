#pragma once

#include "common/PublicTypes.h"

#include <glm/vec3.hpp>
#include <uuid.h>

#include <functional>

class AppData;

/**
 * @brief Render the annotations management window.
 * @param appData Application data containing annotations, image order, and window visibility.
 * @param setViewCameraDirection Callback that points a view camera along an annotation direction.
 * @param paintActiveSegmentationWithActivePolygon Callback that paints the active segmentation from the selected
 * annotation.
 * @param recenterAllViews Callback used by annotation controls that reposition views.
 */
void renderAnnotationWindow(
  AppData& appData,
  const std::function<void(const uuids::uuid& viewUid, const glm::vec3& worldFwdDirection)>& setViewCameraDirection,
  const std::function<void()>& paintActiveSegmentationWithActivePolygon,
  const AllViewsRecenterType& recenterAllViews);
