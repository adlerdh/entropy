#pragma once

#include "logic/interaction/events/ButtonState.h"

/**
 * @brief Scroll wheel or trackpad scroll event passed through interaction handlers
 */
struct WheelEvent
{
  /** @brief Horizontal scroll delta reported by the platform backend */
  float deltaX = 0.0f;
  /** @brief Vertical scroll delta reported by the platform backend */
  float deltaY = 0.0f;
  /** @brief True when the platform reports natural or inverted scrolling */
  bool inverted = false;

  /** @brief Pressed mouse buttons at the time of the wheel event */
  ButtonState buttonState;

  /** @brief True when an interaction handler has consumed the event */
  bool accepted = false;
};
