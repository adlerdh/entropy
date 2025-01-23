#include "logic/interaction/events/ButtonState.h"

#define GLFW_RELEASE 0
#define GLFW_PRESS 1

#define GLFW_MOUSE_BUTTON_LEFT 0
#define GLFW_MOUSE_BUTTON_RIGHT 1
#define GLFW_MOUSE_BUTTON_MIDDLE 2

#define GLFW_MOD_SHIFT 0x0001
#define GLFW_MOD_CONTROL 0x0002
#define GLFW_MOD_ALT 0x0004
#define GLFW_MOD_SUPER 0x0008
#define GLFW_MOD_CAPS_LOCK 0x0010
#define GLFW_MOD_NUM_LOCK 0x0020

// Use -1 to ignore parameter
void ButtonState::updateFromGlfwEvent(int mouseButton, int mouseButtonAction)
{
  if (mouseButton >= 0)
  {
    if (GLFW_MOUSE_BUTTON_LEFT == mouseButton)
    {
      left = (GLFW_PRESS == mouseButtonAction);
    }
    else if (GLFW_MOUSE_BUTTON_RIGHT == mouseButton)
    {
      right = (GLFW_PRESS == mouseButtonAction);
    }
    else if (GLFW_MOUSE_BUTTON_MIDDLE == mouseButton)
    {
      middle = (GLFW_PRESS == mouseButtonAction);
    }
  }
}

// Use -1 to ignore parameter
void ModifierState::updateFromGlfwEvent(int keyMods)
{
  if (keyMods >= 0)
  {
    shift = (GLFW_MOD_SHIFT & keyMods);
    control = (GLFW_MOD_CONTROL & keyMods);
    alt = (GLFW_MOD_ALT & keyMods);
    super = (GLFW_MOD_SUPER & keyMods);
    capsLock = (GLFW_MOD_CAPS_LOCK & keyMods);
    numLock = (GLFW_MOD_NUM_LOCK & keyMods);
  }
}
