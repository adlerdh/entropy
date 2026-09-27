#pragma once

#include "logic/interaction/events/ButtonState.h"

/// Track application mouse-button ownership independently of ImGui capture.
/// Captured presses do not begin application gestures; releases clear ownership even when captured.
/// The caller must invoke cancel() on focus loss.
class PointerGesture
{
public:
  /// Apply a GLFW button event unless the UI captured a non-release event.
  /// @param button GLFW mouse-button identifier.
  /// @param action GLFW action; zero denotes release.
  /// @param mods GLFW modifier bitmask.
  /// @param captured Whether the UI captured this event.
  /// @return True when the event may continue to application interaction handlers.
  bool buttonEvent(int button, int action, int mods, bool captured)
  {
    if (captured && action != 0) return false;
    buttons.updateFromGlfwEvent(button, action);
    modifiers.updateFromGlfwEvent(mods);
    return !captured;
  }
  /// Clear all owned buttons and modifiers, for example when focus is lost.
  void cancel()
  {
    buttons = {};
    modifiers = {};
  }
  /// Return whether the application owns a pressed left, right, or middle button.
  bool dragging() const
  {
    return buttons.left || buttons.right || buttons.middle;
  }
  ButtonState buttons;     ///< Application-owned button state.
  ModifierState modifiers; ///< Modifier state from the most recently accepted event.
};
