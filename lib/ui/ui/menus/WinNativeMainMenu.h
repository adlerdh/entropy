#pragma once

#include "ui/menus/MainMenuBar.h"

struct GLFWwindow;

void updateWindowsNativeMainMenu(GLFWwindow* window, const MainMenuBarCallbacks& callbacks);
void uninstallWindowsNativeMainMenu(GLFWwindow* window);
