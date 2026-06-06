#pragma once

#include <filesystem>

#include <functional>
#include <optional>
#include <string>
#include <vector>

struct GuiData;

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
  std::function<void(const std::filesystem::path& fileName)> loadLayoutsFile;
  std::function<bool(const std::filesystem::path& fileName)> saveLayoutsFile;
  std::function<std::filesystem::path()> defaultLayoutsSaveDirectory;
  std::function<std::string()> defaultLayoutsSaveName;
  std::function<std::vector<std::string>()> layoutNames;
  std::function<std::size_t()> currentLayoutIndex;
  std::function<void(std::size_t)> setCurrentLayoutIndex;
  std::function<void(int)> cycleLayouts;
  std::function<void()> showAbout;
  bool canOpenProject = true;
  bool canAddImage = false;
  bool canAddSegmentation = false;
  bool canSaveProject = false;
  bool canCloseProject = false;
  bool canUseLayouts = false;
};

namespace main_menu
{
void openImage(const MainMenuBarCallbacks& callbacks);
void openProject(const MainMenuBarCallbacks& callbacks);
void addImage(const MainMenuBarCallbacks& callbacks);
void addSegmentation(const MainMenuBarCallbacks& callbacks);
void saveProject(const MainMenuBarCallbacks& callbacks);
void saveProjectAs(const MainMenuBarCallbacks& callbacks);
void closeProject(const MainMenuBarCallbacks& callbacks);
void loadLayouts(const MainMenuBarCallbacks& callbacks);
void saveLayouts(const MainMenuBarCallbacks& callbacks);
} // namespace main_menu

void renderMainMenuBar(GuiData& uiData, const MainMenuBarCallbacks& callbacks);
