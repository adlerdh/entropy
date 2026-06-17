#include "logic/interaction/events/ButtonState.h"

#include <catch2/catch_test_macros.hpp>

namespace
{

constexpr int sk_release = 0;
constexpr int sk_press = 1;

constexpr int sk_leftButton = 0;
constexpr int sk_rightButton = 1;
constexpr int sk_middleButton = 2;
constexpr int sk_unknownButton = 9;

constexpr int sk_shift = 0x0001;
constexpr int sk_control = 0x0002;
constexpr int sk_alt = 0x0004;
constexpr int sk_super = 0x0008;
constexpr int sk_capsLock = 0x0010;
constexpr int sk_numLock = 0x0020;

} // namespace

TEST_CASE("button state tracks individual mouse buttons", "[interaction][button_state]")
{
  ButtonState state;

  state.updateFromGlfwEvent(sk_leftButton, sk_press);
  state.updateFromGlfwEvent(sk_rightButton, sk_press);
  state.updateFromGlfwEvent(sk_middleButton, sk_press);

  CHECK(state.left);
  CHECK(state.right);
  CHECK(state.middle);

  state.updateFromGlfwEvent(sk_rightButton, sk_release);

  CHECK(state.left);
  CHECK_FALSE(state.right);
  CHECK(state.middle);
}

TEST_CASE("button state ignores unknown and negative button events", "[interaction][button_state]")
{
  ButtonState state;
  state.updateFromGlfwEvent(sk_leftButton, sk_press);

  state.updateFromGlfwEvent(sk_unknownButton, sk_press);
  state.updateFromGlfwEvent(-1, sk_release);

  CHECK(state.left);
  CHECK_FALSE(state.right);
  CHECK_FALSE(state.middle);
}

TEST_CASE("modifier state tracks GLFW modifier bits", "[interaction][button_state]")
{
  ModifierState state;

  state.updateFromGlfwEvent(sk_shift | sk_alt | sk_capsLock);

  CHECK(state.shift);
  CHECK_FALSE(state.control);
  CHECK(state.alt);
  CHECK_FALSE(state.super);
  CHECK(state.capsLock);
  CHECK_FALSE(state.numLock);

  state.updateFromGlfwEvent(sk_control | sk_super | sk_numLock);

  CHECK_FALSE(state.shift);
  CHECK(state.control);
  CHECK_FALSE(state.alt);
  CHECK(state.super);
  CHECK_FALSE(state.capsLock);
  CHECK(state.numLock);
}

TEST_CASE("modifier state ignores negative modifier events", "[interaction][button_state]")
{
  ModifierState state;
  state.updateFromGlfwEvent(sk_shift | sk_control);

  state.updateFromGlfwEvent(-1);

  CHECK(state.shift);
  CHECK(state.control);
  CHECK_FALSE(state.alt);
}
