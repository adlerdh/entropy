#ifndef BUTTON_STATE_H
#define BUTTON_STATE_H

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

#endif // BUTTON_STATE_H
