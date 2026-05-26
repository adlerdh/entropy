#pragma once

#include "ui/GuiData.h"
#include "common/Filesystem.h"

#include <functional>

struct MainMenuBarCallbacks
{
  std::function<void(const fs::path& fileName)> openImageFile;
  std::function<void(const fs::path& fileName)> openProjectFile;
  std::function<void()> closeProject;
  bool canOpenProject = true;
  bool canCloseProject = false;
};

void renderMainMenuBar(GuiData& uiData, const MainMenuBarCallbacks& callbacks);
