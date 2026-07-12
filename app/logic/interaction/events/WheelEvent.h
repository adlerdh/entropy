#pragma once

#include "logic/interaction/events/ButtonState.h"

struct WheelEvent
{
  float deltaX = 0.0f;
  float deltaY = 0.0f;
  bool inverted = false;

  ButtonState buttonState;

  bool accepted = false;
};
