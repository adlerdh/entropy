#pragma once

#include "ui/GuiData.h"
#include "common/Filesystem.h"

#include <functional>
#include <optional>
#include <string>

struct MainMenuBarCallbacks
{
  std::function<void(const fs::path& fileName)> openImageFile;
  std::function<void(const fs::path& fileName)> addImageFile;
  std::function<void(const fs::path& fileName)> openProjectFile;
  std::function<bool()> saveProject;
  std::function<bool(const fs::path& fileName)> saveProjectAs;
  std::function<std::optional<fs::path>()> projectFileName;
  std::function<fs::path()> defaultProjectSaveDirectory;
  std::function<std::string()> defaultProjectSaveName;
  std::function<void()> closeProject;
  bool canOpenProject = true;
  bool canAddImage = false;
  bool canSaveProject = false;
  bool canCloseProject = false;
};

void renderMainMenuBar(GuiData& uiData, const MainMenuBarCallbacks& callbacks);
