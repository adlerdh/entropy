#ifndef MOUSE_EVENT_H
#define MOUSE_EVENT_H

#include "logic/interaction/events/ButtonState.h"

struct MouseEvent
{
  float clipPosX = 0.0f;
  float clipPosY = 0.0f;

  ButtonState buttonState;

  bool accepted = false;
};

#endif // MOUSE_EVENT_H
