#pragma once

#include "logic/interaction/events/ButtonState.h"

struct MouseEvent
{
  float clipPosX = 0.0f;
  float clipPosY = 0.0f;

  ButtonState buttonState;

  bool accepted = false;
};
