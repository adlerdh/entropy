#pragma once

#include "logic/interaction/events/ButtonState.h"

/**
 * @brief Mouse position and button state passed through interaction handlers
 */
struct MouseEvent
{
  /** @brief Mouse x position in window clip coordinates */
  float clipPosX = 0.0f;
  /** @brief Mouse y position in window clip coordinates */
  float clipPosY = 0.0f;

  /** @brief Pressed mouse buttons at the time of the event */
  ButtonState buttonState;

  /** @brief True when an interaction handler has consumed the event */
  bool accepted = false;
};
