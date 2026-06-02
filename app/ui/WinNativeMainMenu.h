#pragma once

#include "ui/MainMenuBar.h"

struct GLFWwindow;

void updateWindowsNativeMainMenu(GLFWwindow* window, const MainMenuBarCallbacks& callbacks);
