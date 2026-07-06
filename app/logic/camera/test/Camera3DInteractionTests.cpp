#include "logic/camera/Camera3DInteraction.h"

#include <catch2/catch_test_macros.hpp>

namespace
{

ButtonState buttons(bool left, bool right, bool middle)
{
  return ButtonState{.left = left, .right = right, .middle = middle};
}

ModifierState modifiers(bool alt, bool shift = false)
{
  return ModifierState{.shift = shift, .alt = alt};
}

} // namespace

TEST_CASE("3D camera interaction is allowed in every mouse mode", "[camera][3d]")
{
  CHECK(camera3d::mouseModeAllowsCameraInteraction(MouseMode::Pointer));
  CHECK(camera3d::mouseModeAllowsCameraInteraction(MouseMode::CameraRotate));
  CHECK(camera3d::mouseModeAllowsCameraInteraction(MouseMode::CameraTranslate));
  CHECK(camera3d::mouseModeAllowsCameraInteraction(MouseMode::CameraZoom));
  CHECK(camera3d::mouseModeAllowsCameraInteraction(MouseMode::CrosshairsRotate));
  CHECK(camera3d::mouseModeAllowsCameraInteraction(MouseMode::WindowLevel));
  CHECK(camera3d::mouseModeAllowsCameraInteraction(MouseMode::Segment));
  CHECK(camera3d::mouseModeAllowsCameraInteraction(MouseMode::Annotate));
  CHECK(camera3d::mouseModeAllowsCameraInteraction(MouseMode::ImageTranslate));
  CHECK(camera3d::mouseModeAllowsCameraInteraction(MouseMode::ImageRotate));
  CHECK(camera3d::mouseModeAllowsCameraInteraction(MouseMode::ImageScale));
}

TEST_CASE("3D camera interaction maps buttons and modifiers to drag actions", "[camera][3d]")
{
  CHECK(
    camera3d::dragActionForInput(MouseMode::Pointer, buttons(true, false, false), modifiers(false)) ==
    camera3d::DragAction::Orbit);
  CHECK(
    camera3d::dragActionForInput(MouseMode::Pointer, buttons(false, true, false), modifiers(false)) ==
    camera3d::DragAction::RotateAboutEye);
  CHECK(
    camera3d::dragActionForInput(MouseMode::Pointer, buttons(true, false, false), modifiers(true)) ==
    camera3d::DragAction::RotateAboutEye);
  CHECK(
    camera3d::dragActionForInput(MouseMode::Pointer, buttons(true, false, false), modifiers(false), true) ==
    camera3d::DragAction::RotateAboutEye);
  CHECK(
    camera3d::dragActionForInput(MouseMode::Pointer, buttons(true, false, false), modifiers(true), true) ==
    camera3d::DragAction::Orbit);
  CHECK(
    camera3d::dragActionForInput(MouseMode::Pointer, buttons(true, false, false), modifiers(false, true)) ==
    camera3d::DragAction::Pan);
  CHECK(
    camera3d::dragActionForInput(MouseMode::Pointer, buttons(true, false, false), modifiers(true, true)) ==
    camera3d::DragAction::Roll);
  CHECK(
    camera3d::dragActionForInput(MouseMode::Pointer, buttons(false, false, true), modifiers(false)) ==
    camera3d::DragAction::Pan);

  CHECK(
    camera3d::dragActionForInput(MouseMode::CameraRotate, buttons(true, false, false), modifiers(false)) ==
    camera3d::DragAction::Orbit);
  CHECK(
    camera3d::dragActionForInput(MouseMode::CameraRotate, buttons(true, false, false), modifiers(true)) ==
    camera3d::DragAction::RotateAboutEye);
  CHECK(
    camera3d::dragActionForInput(MouseMode::CrosshairsRotate, buttons(true, false, false), modifiers(false), true) ==
    camera3d::DragAction::RotateAboutEye);
  CHECK(
    camera3d::dragActionForInput(MouseMode::CameraZoom, buttons(true, false, false), modifiers(false)) ==
    camera3d::DragAction::Orbit);
  CHECK(
    camera3d::dragActionForInput(MouseMode::WindowLevel, buttons(true, false, false), modifiers(true, true)) ==
    camera3d::DragAction::Roll);
  CHECK(
    camera3d::dragActionForInput(MouseMode::Segment, buttons(true, false, false), modifiers(false)) ==
    camera3d::DragAction::Orbit);
  CHECK(
    camera3d::dragActionForInput(MouseMode::ImageTranslate, buttons(false, true, false), modifiers(false)) ==
    camera3d::DragAction::RotateAboutEye);
}

TEST_CASE("3D camera translate mode is pan-focused", "[camera][3d]")
{
  CHECK(
    camera3d::dragActionForInput(MouseMode::CameraTranslate, buttons(true, false, false), modifiers(false)) ==
    camera3d::DragAction::Pan);
  CHECK(
    camera3d::dragActionForInput(MouseMode::CameraTranslate, buttons(false, false, true), modifiers(false)) ==
    camera3d::DragAction::Pan);
  CHECK(
    camera3d::dragActionForInput(MouseMode::CameraTranslate, buttons(false, true, false), modifiers(false)) ==
    camera3d::DragAction::RotateAboutEye);
}

TEST_CASE("3D camera interaction ignores inactive buttons", "[camera][3d]")
{
  CHECK_FALSE(camera3d::dragActionForInput(MouseMode::Pointer, buttons(false, false, false), modifiers(false)));
}
