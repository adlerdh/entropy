#pragma once

#include "logic/app/State.h"
#include "logic/interaction/events/ButtonState.h"

#include <optional>

namespace camera3d
{

enum class DragAction
{
  Orbit,
  RotateAboutEye,
  Roll,
  Pan
};

bool mouseModeAllowsCameraInteraction(MouseMode mouseMode);

std::optional<DragAction> dragActionForInput(
  MouseMode mouseMode,
  const ButtonState& buttons,
  const ModifierState& modifiers,
  bool viewPositionFollowsCrosshairs = false);

} // namespace camera3d
