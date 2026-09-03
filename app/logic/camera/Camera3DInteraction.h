#pragma once

#include "logic/app/State.h"
#include "logic/interaction/events/ButtonState.h"

#include <optional>

namespace camera3d
{

/**
 * @brief Camera drag operation selected from mouse buttons and keyboard modifiers
 */
enum class DragAction
{
  /** @brief Rotate the camera around the selected orbit target */
  Orbit,
  /** @brief Rotate the camera around its current eye position */
  RotateAboutEye,
  /** @brief Rotate the camera in the view plane */
  Roll,
  /** @brief Translate the camera parallel to the view plane */
  Pan
};

/**
 * @brief Check whether a mouse mode permits double-click surface picking in a 3D view.
 *
 * Picking is available in the general pointer/navigation/display-adjustment modes whose double-click gesture is not
 * otherwise assigned. Transformation, annotation, and crosshairs-rotation modes remain excluded.
 */
bool mouseModeAllowsPointPicking(MouseMode mouseMode) noexcept;

/**
 * @brief Resolve the 3D camera drag action for the current input state
 * @param mouseMode Active application mouse mode
 * @param buttons Current mouse button state
 * @param modifiers Current keyboard modifier state
 * @param viewPositionFollowsCrosshairs True when left drag defaults to rotate-about-eye behavior
 * @return Drag action to perform, or `std::nullopt` when no camera drag action is active
 */
std::optional<DragAction> dragActionForInput(
  MouseMode mouseMode,
  const ButtonState& buttons,
  const ModifierState& modifiers,
  bool viewPositionFollowsCrosshairs = false) noexcept;

} // namespace camera3d
