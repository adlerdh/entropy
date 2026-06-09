#pragma once

#include "common/InputParams.h"
#include <filesystem>

#include "logic/app/CallbackHandler.h"
#include "logic/app/Data.h"
#include "logic/app/Settings.h"
#include "logic/app/State.h"
#include "logic/ipc/SnapCursorSync.h"

#include "rendering/Rendering.h"
#include "ui/ImGuiWrapper.h"

#include "windowing/GlfwWrapper.h"

#include <uuid.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <future>
#include <optional>
#include <string>
#include <vector>

/**
 * @brief This class basically runs the show. Its responsibilities are
 * 1) Hold the OpenGL context and all application data, including for the UI, rendering, and
 * windowing 2) Run the rendering loop 3) Load images 4) Execute callbacks from the UI
 *
 * @note Might be nice to split this class apart.
 */
class EntropyApp
{
public:
  EntropyApp();
  ~EntropyApp();

  /// Initialize rendering functions, OpenGL context, and windowing (GLFW)
  void init();

  /// Run the render loop
  void run();

  /// Resize the window, with width and height specified in artificial units that do not
  /// necessarily correspond to real screen pixels, as is the case when DPI scaling is activated.
  /// @param[in] windowWidth Window width (device-agnostic units)
  /// @param[in] windowHeight Window height (device-agnostic units)
  void resize(int windowWidth, int windowHeight);

  /// Render one frame
  void render();

  /// Asynchronously load images and notify render loop when done
  void loadImagesFromParams(const InputParams&);
  void loadImageFile(const std::filesystem::path& fileName);
  void loadImageFiles(const std::vector<std::filesystem::path>& fileNames);
  void addImageFile(const std::filesystem::path& fileName);
  void addImageFiles(const std::vector<std::filesystem::path>& fileNames);
  void handleDroppedFiles(const std::vector<std::filesystem::path>& fileNames);
  void addSegmentationFile(const std::filesystem::path& fileName);
  void addSegmentationFileToImage(const std::filesystem::path& fileName, const uuids::uuid& imageUid);
  void loadProjectFile(const std::filesystem::path& fileName);
  bool saveProject();
  bool saveProjectAs(const std::filesystem::path& fileName);
  void loadLayoutsFile(const std::filesystem::path& fileName);
  bool saveLayoutsFile(const std::filesystem::path& fileName);
  bool setReferenceImage(const uuids::uuid& imageUid);
  bool removeImage(const uuids::uuid& imageUid);
  void requestCloseProject();
  void requestQuitApp();
  void quitAppWithoutPrompt();
  void closeProject();

  /**
   * @brief Load a serialized image from disk
   * @param image Serialized image structure
   * @param isReferenceImage Flag indicating whether this is the reference image
   * @return True iff the image was successfully loaded
   */
  bool loadSerializedImage(const serialize::Image& image, bool isReferenceImage);

  /// Load a segmentation from disk. If its header does not match the given image, then it is not
  /// loaded
  /// @return Uid and flag if loaded.
  /// False indcates that it was already loaded and that we are returning an existing image.
  std::pair<std::optional<uuids::uuid>, bool> loadSegmentation(
    const std::filesystem::path& fileName,
    const std::optional<uuids::uuid>& imageUid = std::nullopt);

  /**
   * @brief Load a deformation field from disk.
   * @return UID and flag if loaded. False indcates that it was already loaded and that we are
   * returning an existing image.
   *
   * @todo If its header does not match the given image, then it is not loaded
   */
  std::pair<std::optional<uuids::uuid>, bool> loadDeformationField(const std::filesystem::path& fileName);

  CallbackHandler& callbackHandler();

  const AppData& appData() const;
  AppData& appData();

  const AppSettings& appSettings() const;
  AppSettings& appSettings();

  const AppState& appState() const;
  AppState& appState();

  const GuiData& guiData() const;
  GuiData& guiData();

  const GlfwWrapper& glfw() const;
  GlfwWrapper& glfw();

  const ImGuiWrapper& imgui() const;
  ImGuiWrapper& imgui();

  const RenderData& renderData() const;
  RenderData& renderData();

  const WindowData& windowData() const;
  WindowData& windowData();

  static void logPreamble();

private:
  //    void broadcastCrosshairsPosition();

  void setCallbacks();

  /// Function called when images have been loaded from disk
  void onImagesReady();
  void startAsyncImageLoad(
    std::string windowTitleStatus,
    std::function<bool()> loadTask,
    std::function<void()> onLoadFailed,
    bool showLoadingOverlay = true);
  void beginLoadProject(serialize::EntropyProject project, std::optional<std::filesystem::path> projectFileName);
  void continueLargeImageProjectPreflight();
  void handleLargeImageLoadDecision(GuiData::LargeImageLoadDecision decision);
  bool loadProject(const serialize::EntropyProject& project);
  serialize::EntropyProject createProjectSnapshot() const;
  serialize::Image createImageSnapshot(const uuids::uuid& imageUid) const;
  bool projectHasUnsavedChanges() const;
  bool hasUnsavedAnnotations() const;
  bool saveDirtyAnnotationsWithDialogs();
  bool saveAnnotationsForImage(const uuids::uuid& imageUid, const std::filesystem::path& fileName);
  void markProjectSavedSnapshot();
  std::string windowTitleStatus() const;
  void updateWindowTitleStatus();

  /// Load an image from disk.
  /// @return Uid and flag if loaded.
  /// False indcates that it was already loaded and that we are returning an existing image.
  std::pair<std::optional<uuids::uuid>, bool> loadImage(
    const std::filesystem::path& fileName,
    bool ignoreIfAlreadyLoaded);

  std::future<void> m_futureLoadProject;

  /// Atomic boolean that is set to true iff image loading is cancelled
  std::atomic<bool> m_imageLoadCancelled;

  /// Atomic boolean set to true when all project images are loaded from disk and
  /// ready to be loaded into textures
  std::atomic<bool> m_imagesReady;

  /// Atomic boolean set to true iff images could not be loaded.
  /// If true, this flag will cause the render loop to exit.
  std::atomic<bool> m_imageLoadFailed;

  /// True when onImagesReady is handling a live Add Image operation instead of initial load.
  bool m_preserveLayoutsOnImagesReady = false;

  /// Images added by the current live Add Image operation, if any.
  std::vector<uuids::uuid> m_pendingAddedImageUids;
  std::optional<std::filesystem::path> m_pendingLayoutsFile = std::nullopt;

  enum class LargeImageLoadContext : std::uint8_t
  {
    None,
    AddImage,
    Project
  };

  LargeImageLoadContext m_pendingLargeImageLoadContext = LargeImageLoadContext::None;
  std::optional<std::filesystem::path> m_pendingLargeAddImageFile = std::nullopt;
  std::optional<serialize::EntropyProject> m_pendingLargeProject = std::nullopt;
  std::optional<std::filesystem::path> m_pendingLargeProjectFileName = std::nullopt;
  std::size_t m_pendingLargeProjectImageIndex = 0;
  std::optional<serialize::EntropyProject> m_savedProjectSnapshot = std::nullopt;

  GlfwWrapper m_glfw;                //!< GLFW wrapper
  AppData m_data;                    //!< Application data
  Rendering m_rendering;             //!< Render logic
  CallbackHandler m_callbackHandler; //!< UI callback handlers
  SnapCursorSync m_snapCursorSync;   //!< Cursor synchronization with ITK-SNAP IPC
  ImGuiWrapper m_imgui;              //!< ImGui wrapper
};
