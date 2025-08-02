#pragma once

struct ButtonState
{
  void updateFromGlfwEvent(int mouseButton, int mouseButtonAction);

  bool left = false;
  bool right = false;
  bool middle = false;
};

struct ModifierState
{
  void updateFromGlfwEvent(int keyMods);

  bool shift = false;
  bool control = false;
  bool alt = false;
  bool super = false;
  bool capsLock = false;
  bool numLock = false;
};
