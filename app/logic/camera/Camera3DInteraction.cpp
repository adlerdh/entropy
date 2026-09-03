#include "logic/camera/Camera3DInteraction.h"

namespace camera3d
{

bool mouseModeAllowsPointPicking(const MouseMode mouseMode) noexcept
{
  switch (mouseMode) {
    case MouseMode::Pointer:
    case MouseMode::CameraZoom:
    case MouseMode::WindowLevel:
    case MouseMode::CameraTranslate:
    case MouseMode::Segment:
      return true;

    case MouseMode::CameraRotate:
    case MouseMode::CrosshairsRotate:
    case MouseMode::Annotate:
    case MouseMode::ImageTranslate:
    case MouseMode::ImageRotate:
    case MouseMode::ImageScale:
      return false;
  }

  return false;
}

std::optional<DragAction> dragActionForInput(
  MouseMode mouseMode,
  const ButtonState& buttons,
  const ModifierState& modifiers,
  bool viewPositionFollowsCrosshairs) noexcept
{
  const auto generalNavigationAction = [&]() -> std::optional<DragAction> {
    if (buttons.middle) {
      return DragAction::Pan;
    }

    if (buttons.left) {
      if (modifiers.alt && modifiers.shift) {
        return DragAction::Roll;
      }
      if (modifiers.shift) {
        return DragAction::Pan;
      }
      const bool rotateAboutEye = viewPositionFollowsCrosshairs ? !modifiers.alt : modifiers.alt;
      return rotateAboutEye ? DragAction::RotateAboutEye : DragAction::Orbit;
    }

    if (buttons.right) {
      if (modifiers.alt && modifiers.shift) {
        return DragAction::Roll;
      }
      return DragAction::RotateAboutEye;
    }

    return std::nullopt;
  };

  switch (mouseMode) {
    case MouseMode::Pointer:
    case MouseMode::CameraRotate:
    case MouseMode::CrosshairsRotate:
    case MouseMode::CameraZoom:
    case MouseMode::WindowLevel:
    case MouseMode::Segment:
    case MouseMode::Annotate:
    case MouseMode::ImageTranslate:
    case MouseMode::ImageRotate:
    case MouseMode::ImageScale:
      return generalNavigationAction();

    case MouseMode::CameraTranslate:
      if (buttons.left || buttons.middle) {
        return DragAction::Pan;
      }
      if (buttons.right) {
        return DragAction::RotateAboutEye;
      }
      return std::nullopt;
  }

  return std::nullopt;
}

} // namespace camera3d
