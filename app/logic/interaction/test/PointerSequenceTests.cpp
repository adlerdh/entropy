#include "logic/interaction/PointerGesture.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

TEST_CASE("Pointer press drag capture release resume never leaves an owned button", "[workflow][interaction]")
{
  const int button = GENERATE(0, 1, 2);
  PointerGesture gesture;
  unsigned modelChanges = 0;
  const auto move = [&] {
    if (gesture.dragging()) ++modelChanges;
  };
  REQUIRE(gesture.buttonEvent(button, 1, 1, false));
  move();
  REQUIRE(modelChanges == 1);
  CHECK_FALSE(gesture.buttonEvent(button, 0, 0, true));
  CHECK_FALSE(gesture.dragging());
  move();
  CHECK(modelChanges == 1);
  CHECK_FALSE(gesture.modifiers.shift);
  gesture.buttonEvent(button, 1, 0, true);
  CHECK_FALSE(gesture.dragging());
  move();
  CHECK(modelChanges == 1);
  REQUIRE(gesture.buttonEvent(button, 1, 0, false));
  move();
  CHECK(modelChanges == 2);
}

TEST_CASE("Focus loss and removed layout cancel all gesture buttons and modifiers", "[workflow][interaction]")
{
  PointerGesture gesture;
  for (int button : {0, 1, 2})
    REQUIRE(gesture.buttonEvent(button, 1, 7, false));
  gesture.cancel();
  CHECK_FALSE(gesture.dragging());
  CHECK_FALSE(gesture.modifiers.shift);
  CHECK_FALSE(gesture.modifiers.control);
  CHECK_FALSE(gesture.modifiers.alt);
  CHECK_FALSE(gesture.buttonEvent(0, 0, 0, true));
  CHECK_FALSE(gesture.dragging());
}
