#pragma once

#include "logic/interaction/events/ButtonState.h"

// Mouse ownership is independent of ImGui capture. UI presses must not become
// application gestures; every release and focus loss must still clear ownership.
class PointerGesture
{
public:
  bool buttonEvent(int button, int action, int mods, bool captured)
  {
    if (captured && action != 0) return false;
    buttons.updateFromGlfwEvent(button, action);
    modifiers.updateFromGlfwEvent(mods);
    return !captured;
  }
  void cancel()
  {
    buttons = {};
    modifiers = {};
  }
  bool dragging() const
  {
    return buttons.left || buttons.right || buttons.middle;
  }
  ButtonState buttons;
  ModifierState modifiers;
};
