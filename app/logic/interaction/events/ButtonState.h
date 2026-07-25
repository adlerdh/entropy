#pragma once

/**
 * @brief Current pressed state for mouse buttons used by interaction handlers
 */
struct ButtonState
{
  /**
   * @brief Apply one GLFW mouse button event to the stored button state
   * @param mouseButton GLFW mouse button identifier, or a negative value to ignore the button
   * @param mouseButtonAction GLFW mouse action value
   */
  void updateFromGlfwEvent(int mouseButton, int mouseButtonAction) noexcept;

  /** @brief True when the left mouse button is pressed */
  bool left = false;
  /** @brief True when the right mouse button is pressed */
  bool right = false;
  /** @brief True when the middle mouse button is pressed */
  bool middle = false;
};

/**
 * @brief Current keyboard modifier state used together with mouse and wheel events
 */
struct ModifierState
{
  /**
   * @brief Apply one GLFW modifier bit mask to the stored modifier state
   * @param keyMods GLFW modifier bit mask, or a negative value to ignore the update
   */
  void updateFromGlfwEvent(int keyMods) noexcept;

  /** @brief True when Shift is active */
  bool shift = false;
  /** @brief True when Control is active */
  bool control = false;
  /** @brief True when Alt or Option is active */
  bool alt = false;
  /** @brief True when Super, Command, or Windows is active */
  bool super = false;
  /** @brief True when Caps Lock is active */
  bool capsLock = false;
  /** @brief True when Num Lock is active */
  bool numLock = false;
};
