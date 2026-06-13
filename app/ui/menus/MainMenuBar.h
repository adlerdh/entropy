#pragma once

#include <filesystem>

#include <functional>
#include <optional>
#include <string>
#include <vector>

struct GuiData;

enum class MainMenuAction
{
  SetModePointer,
  SetModeWindowLevel,
  SetModeZoom,
  SetModePan,
  SetModeRotateView,
  SetModeRotateCrosshairs,
  SetModeSegment,
  SetModeAnnotate,
  SetModeTranslateImage,
  SetModeRotateImage,
  Recenter,
  ResetView,
  ToggleImageVisibility,
  ToggleSegmentationVisibility,
  ToggleImageEdges,
  ToggleSegmentationOutline,
  DecreaseActiveImageOpacity,
  IncreaseActiveImageOpacity,
  DecreaseSegmentationOpacity,
  IncreaseSegmentationOpacity,
  ToggleScaleBars,
  ToggleLightboxOffsets,
  ToggleOverlays,
  ToggleFullScreen,
  ToggleSync,
  ToggleSyncSendCursor,
  ToggleSyncReceiveCursor,
  ToggleSyncSendZoom,
  ToggleSyncReceiveZoom,
  ToggleSyncSendPan,
  ToggleSyncReceivePan,
  SetActiveImageAsReference,
  ExportActiveImage,
  RemoveActiveImage,
  MoveActiveImageBackward,
  MoveActiveImageForward,
  MoveActiveImageToBack,
  MoveActiveImageToFront,
  ToggleActiveImageTransformationLock,
  ResetActiveImageManualTransformation,
  SaveActiveImageManualTransformation,
  SaveActiveImageInitialAndManualTransformation,
  ShowOpacityMixer,
  CreateSegmentation,
  SaveSegmentation,
  ClearSegmentation,
  RemoveSegmentation,
  PreviousForegroundLabel,
  NextForegroundLabel,
  PreviousBackgroundLabel,
  NextBackgroundLabel,
  DecreaseBrushSize,
  IncreaseBrushSize,
  PaintSegmentationFromAnnotation,
  SaveAnnotations,
  RemoveAnnotation,
  MoveAnnotationBackward,
  MoveAnnotationForward,
  MoveAnnotationToBack,
  MoveAnnotationToFront,
  CreateLandmarkGroup,
  SaveLandmarkGroup,
  AddLandmark,
  AddLayout,
  RemoveLayout,
  ToggleImagesWindow,
  ToggleSegmentationsWindow,
  ToggleLandmarksWindow,
  ToggleAnnotationsWindow,
  ToggleIsosurfacesWindow,
  ToggleSettingsWindow,
  ShowSettingsWindow,
  ShowSynchronizeSettingsWindow,
  ToggleInspectorWindow,
  ToggleOpacityMixerWindow,
  ToggleImGuiDemoWindow,
  ToggleImPlotDemoWindow,
  ToggleToolbar
};

struct MainMenuBarCallbacks
{
  std::function<void(const std::vector<std::filesystem::path>& fileNames)> openImageFiles;
  std::function<void(const std::vector<std::filesystem::path>& fileNames)> addImageFiles;
  std::function<void(const std::vector<std::filesystem::path>& folderNames)> openDicomFolders;
  std::function<void()> requestDicomFolderPathDialog;
  std::function<void(const std::filesystem::path& fileName)> addSegmentationFile;
  std::function<void(const std::filesystem::path& fileName)> openProjectFile;
  std::function<bool()> saveProject;
  std::function<bool(const std::filesystem::path& fileName)> saveProjectAs;
  std::function<std::optional<std::filesystem::path>()> projectFileName;
  std::function<std::filesystem::path()> defaultProjectSaveDirectory;
  std::function<std::string()> defaultProjectSaveName;
  std::function<void()> closeProject;
  std::function<void()> quitApp;
  std::function<void(const std::filesystem::path& fileName)> loadLayoutsFile;
  std::function<bool(const std::filesystem::path& fileName)> saveLayoutsFile;
  std::function<std::filesystem::path()> defaultLayoutsSaveDirectory;
  std::function<std::string()> defaultLayoutsSaveName;
  std::function<std::vector<std::string>()> layoutNames;
  std::function<std::size_t()> currentLayoutIndex;
  std::function<void(std::size_t)> setCurrentLayoutIndex;
  std::function<void(int)> cycleLayouts;
  std::function<std::vector<std::string>()> imageNames;
  std::function<std::size_t()> activeImageIndex;
  std::function<void(std::size_t)> setActiveImageIndex;
  std::function<void(MainMenuAction)> performAction;
  std::function<bool(MainMenuAction)> isActionEnabled;
  std::function<bool(MainMenuAction)> isActionChecked;
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
void openDicomSeries(const MainMenuBarCallbacks& callbacks);
void addSegmentation(const MainMenuBarCallbacks& callbacks);
void saveProject(const MainMenuBarCallbacks& callbacks);
void saveProjectAs(const MainMenuBarCallbacks& callbacks);
void closeProject(const MainMenuBarCallbacks& callbacks);
void quitApp(const MainMenuBarCallbacks& callbacks);
void loadLayouts(const MainMenuBarCallbacks& callbacks);
void saveLayouts(const MainMenuBarCallbacks& callbacks);
bool actionEnabled(const MainMenuBarCallbacks& callbacks, MainMenuAction action);
bool actionChecked(const MainMenuBarCallbacks& callbacks, MainMenuAction action);
void performAction(const MainMenuBarCallbacks& callbacks, MainMenuAction action);
} // namespace main_menu

void renderMainMenuBar(GuiData& uiData, const MainMenuBarCallbacks& callbacks);
