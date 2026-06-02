#pragma once

#include "ui/GuiData.h"
#include <filesystem>

#include <functional>
#include <optional>
#include <string>

struct MainMenuBarCallbacks
{
  std::function<void(const std::filesystem::path& fileName)> openImageFile;
  std::function<void(const std::filesystem::path& fileName)> addImageFile;
  std::function<void(const std::filesystem::path& fileName)> addSegmentationFile;
  std::function<void(const std::filesystem::path& fileName)> openProjectFile;
  std::function<bool()> saveProject;
  std::function<bool(const std::filesystem::path& fileName)> saveProjectAs;
  std::function<std::optional<std::filesystem::path>()> projectFileName;
  std::function<std::filesystem::path()> defaultProjectSaveDirectory;
  std::function<std::string()> defaultProjectSaveName;
  std::function<void()> closeProject;
  bool canOpenProject = true;
  bool canAddImage = false;
  bool canAddSegmentation = false;
  bool canSaveProject = false;
  bool canCloseProject = false;
};

void renderMainMenuBar(GuiData& uiData, const MainMenuBarCallbacks& callbacks);
