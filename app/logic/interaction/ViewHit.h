#pragma once

#include "windowing/View.h"

#include <glm/glm.hpp>
#include <uuid.h>

#include <optional>

class AppData;

/**
 * @brief Information about a view hit by a pointer position
 */
struct ViewHit
{
  /** @brief Non-owning pointer to the view that was hit */
  View* view = nullptr;
  /** @brief UID of the view that was hit */
  uuids::uuid viewUid;

  /** @brief Hit position in window clip coordinates */
  glm::vec2 windowClipPos{0.0f};
  /** @brief Hit position in the hit view's clip coordinates */
  glm::vec2 viewClipPos{0.0f};
  /** @brief World-space hit position after undoing lightbox slice offset and snapping */
  glm::vec4 worldPos{0.0f};
  /** @brief World-space hit position before undoing lightbox slice offset */
  glm::vec4 worldPos_offsetApplied{0.0f};
  /** @brief World-space front direction of the view used for hit geometry */
  glm::vec3 worldFrontAxis{0.0f, 0.0f, 1.0f};
};

/**
 * @brief Resolve the current view and world position under a window-space pointer coordinate
 * @param appData Application data containing layouts, cameras, images, and crosshairs state
 * @param windowPos Pointer position in window pixel coordinates
 * @param viewUidForOverride Optional view UID used instead of hit testing the current cursor position
 * @return Hit information when a valid rendered view is hit; otherwise `std::nullopt`
 * @throw Propagates exceptions from application data or camera math access
 *
 * @todo Interface instead of `appData`
 */
std::optional<ViewHit> getViewHit(
  AppData& appData,
  const glm::vec2& windowPos,
  const std::optional<uuids::uuid>& viewUidForOverride = std::nullopt);
