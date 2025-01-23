#ifndef WHEEL_EVENT_H
#define WHEEL_EVENT_H

#include "logic/interaction/events/ButtonState.h"

struct WheelEvent
{
  float deltaX = 0.0f;
  float deltaY = 0.0f;
  bool inverted = false;

  ButtonState buttonState;

  bool accepted = false;
};

#endif // WHEEL_EVENT_H
