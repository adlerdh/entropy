#pragma once

#include "common/InputParams.h"
#include "image/DicomSeries.h"
#include <filesystem>

#include "logic/app/CallbackHandler.h"
#include "logic/app/Data.h"
#include "logic/app/Settings.h"
#include "logic/app/State.h"
#include "logic/sync/EntropyInstanceSync.h"
#include "logic/sync/ItkSnapSync.h"

#include "rendering/Rendering.h"
#include "ui/ImGuiWrapper.h"
#include "viewer/ViewTypes.h"

#include "windowing/GlfwWrapper.h"

#include <uuid.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <future>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @brief Top-level runtime object for the Entropy desktop application.
 *
 * EntropyApp owns the window, rendering backend, UI wrapper, synchronization services, and
 * application data. It coordinates startup, the render loop, project/image loading, and
 * high-level UI callbacks.
 */
class EntropyApp
{
public:
  /** @brief Construct the application services that depend on the GLFW/OpenGL context. */
  EntropyApp();

  /** @brief Stop background loading and synchronization services before shutdown. */
  ~EntropyApp();

  /**
   * @brief Initialize GLFW, rendering resources, UI callbacks, and user preferences.
   * @throws Propagates initialization failures from windowing, OpenGL, or rendering setup.
   */
  void init();

  /**
   * @brief Run the main event/render loop until the application should close.
   * @throws Does not intentionally throw.
   */
  void run();

  /**
   * @brief Resize the application window and render viewport.
   * @param windowWidth Window width in device-independent units.
   * @param windowHeight Window height in device-independent units.
   * @throws Does not intentionally throw.
   */
  void resize(int windowWidth, int windowHeight);

  /**
   * @brief Render one frame and process deferred UI/application work.
   * @throws Does not intentionally throw.
   */
  void render();

  /**
   * @brief Load startup inputs parsed from the command line.
   * @param params Parsed startup parameters.
   * @throws Does not intentionally throw; load failures are reported through application state.
   */
  void loadImagesFromParams(const InputParams& params);

  /** @brief Replace the current project with one image file. */
  void loadImageFile(const std::filesystem::path& fileName);

  /** @brief Replace the current project with image files. */
  void loadImageFiles(const std::vector<std::filesystem::path>& fileNames);

  /** @brief Add one image file to the current project. */
  void addImageFile(const std::filesystem::path& fileName);

  /** @brief Add image files to the current project. */
  void addImageFiles(const std::vector<std::filesystem::path>& fileNames);

  /** @brief Open or add dropped files according to their type and current project state. */
  void handleDroppedFiles(const std::vector<std::filesystem::path>& fileNames);

  /** @brief Replace the current project with DICOM series discovered from folders or files. */
  void openDicomSeriesFolders(const std::vector<std::filesystem::path>& folderNames);

  /** @brief Add a segmentation file to the active image. */
  void addSegmentationFile(const std::filesystem::path& fileName);

  /** @brief Add a segmentation file to a specific image. */
  void addSegmentationFileToImage(const std::filesystem::path& fileName, const uuids::uuid& imageUid);

  /** @brief Import outputs from a completed registration job through the async loading pipeline. */
  void importRegistrationJobOutputs(const std::string& jobId);

  /** @brief Replace the current project with a serialized project file. */
  void loadProjectFile(const std::filesystem::path& fileName);

  /**
   * @brief Save the current project to its existing project path.
   * @return True when the project was saved.
   */
  bool saveProject();

  /**
   * @brief Save the current project to a specific path.
   * @param fileName Destination project path.
   * @return True when the project was saved.
   */
  bool saveProjectAs(const std::filesystem::path& fileName);

  /** @brief Replace the current layout list from a layout JSON file. */
  void loadLayoutsFile(const std::filesystem::path& fileName);

  /**
   * @brief Save the current layout list to a layout JSON file.
   * @return True when the layouts were saved.
   */
  bool saveLayoutsFile(const std::filesystem::path& fileName);

  /**
   * @brief Make an image the project reference image.
   * @return True when the reference image changed.
   */
  bool setReferenceImage(const uuids::uuid& imageUid);

  /**
   * @brief Remove an image and its dependent project data.
   * @return True when the image was removed.
   */
  bool removeImage(const uuids::uuid& imageUid);

  /** @brief Request project close, prompting first when there are unsaved changes. */
  void requestCloseProject();

  /** @brief Request application quit, prompting first when there are unsaved changes. */
  void requestQuitApp();

  /** @brief Quit without an unsaved-project prompt. */
  void quitAppWithoutPrompt();

  /** @brief Close the current project and reset project-owned state. */
  void closeProject();

  /** @brief Reset project-wide settings without removing loaded data or transforms. */
  void resetProjectSettings();

  /** @brief Continue the pending action selected before the unsaved-project prompt appeared. */
  void continueAfterUnsavedProjectPrompt();

  /**
   * @brief Load one serialized image and its related project data.
   * @param image Serialized image structure.
   * @param isReferenceImage True when this image should become the reference image.
   * @param resolvedDicomSeries Already-discovered DICOM series to load, or nullptr to resolve from
   * serialized metadata.
   * @return True when the image and dependent data were loaded.
   * @throws Propagates image-loading exceptions from the image library.
   */
  bool loadSerializedImage(
    const serialize::Image& image,
    bool isReferenceImage,
    const dicom::SeriesInfo* resolvedDicomSeries = nullptr);

  /**
   * @brief Load a segmentation from disk.
   * @param fileName Segmentation image path.
   * @param imageUid Optional image UID to pair with the segmentation.
   * @return Segmentation UID and true when loaded; existing UID and false when already loaded.
   * @throws Propagates image-loading exceptions from the image library.
   */
  std::pair<std::optional<uuids::uuid>, bool> loadSegmentation(
    const std::filesystem::path& fileName,
    const std::optional<uuids::uuid>& imageUid = std::nullopt);

  /**
   * @brief Load a warp field from disk.
   * @param fileName Warp-field image path.
   * @return Warp-field UID and true when loaded; existing UID and false when already loaded.
   * @throws Propagates image-loading exceptions from the image library.
   */
  std::pair<std::optional<uuids::uuid>, bool> loadDeformationField(const std::filesystem::path& fileName);

  /**  Load a warp field asynchronously and assign it to an image when loading completes. */
  void loadAndAssignDeformationField(
    const uuids::uuid& imageUid,
    const std::filesystem::path& fileName,
    bool forwardWarp,
    std::optional<uuids::uuid> inverseWarpReferenceImageUid = std::nullopt);

  /** @brief Access UI callback operations. */
  CallbackHandler& callbackHandler();

  /** @brief Access immutable application data. */
  const AppData& appData() const;

  /** @brief Access mutable application data. */
  AppData& appData();

  /** @brief Access immutable application settings. */
  const AppSettings& appSettings() const;

  /** @brief Access mutable application settings. */
  AppSettings& appSettings();

  /** @brief Access immutable runtime state. */
  const AppState& appState() const;

  /** @brief Access mutable runtime state. */
  AppState& appState();

  /** @brief Access immutable transient UI state. */
  const GuiData& guiData() const;

  /** @brief Access mutable transient UI state. */
  GuiData& guiData();

  /** @brief Access immutable GLFW/window services. */
  const GlfwWrapper& glfw() const;

  /** @brief Access mutable GLFW/window services. */
  GlfwWrapper& glfw();

  /** @brief Access immutable ImGui services. */
  const ImGuiWrapper& imgui() const;

  /** @brief Access mutable ImGui services. */
  ImGuiWrapper& imgui();

  /** @brief Access immutable layout/window state. */
  const WindowData& windowData() const;

  /** @brief Access mutable layout/window state. */
  WindowData& windowData();

  /** @brief Log build, host, compiler, and application metadata at startup. */
  static void logPreamble();

private:
  /** @brief Wire UI, window, and rendering callbacks to application operations. */
  void setCallbacks();

  /**
   * @brief Complete an async DICOM discovery task and open the series-selection UI.
   * @throws Does not intentionally throw; discovery exceptions are converted to warnings.
   */
  void pollDicomSeriesScan();

  /**
   * @brief Start asynchronous DICOM discovery for folders or slice files.
   * @param inputPaths Folders or DICOM files to scan recursively.
   * @param addToExistingProject True when selected series should be added to the loaded project.
   * @throws Does not intentionally throw.
   */
  void beginDicomSeriesScan(const std::vector<std::filesystem::path>& inputPaths, bool addToExistingProject);

  /**
   * @brief Load selected DICOM series through the normal image setup path.
   * @param series Series selected in the DICOM selection dialog.
   * @param referenceSeriesIndex Selected-series index to promote to reference, when allowed.
   * @param addToExistingProject True to append images instead of replacing the current project.
   * @throws Does not intentionally throw; load failures are logged and reported through app state.
   */
  void loadDicomSeries(
    const std::vector<dicom::SeriesInfo>& series,
    std::optional<std::size_t> referenceSeriesIndex,
    bool addToExistingProject);

  /**
   * @brief Finalize images after background loading completes.
   * @throws Does not intentionally throw; load failures are logged and reflected in app state.
   */
  void onImagesReady();

  /**
   * @brief Run an image/project load task on a background thread.
   * @param windowTitleStatus Temporary status text for the window title.
   * @param loadTask Background task that returns true on success.
   * @param onLoadFailed Main-thread callback when loading fails.
   * @param showLoadingOverlay True to show a loading
   * overlay while the task runs.
   * @param loadingItems Items to show in the loading-status popup.
   * @param
   * loadingStatusTitle Title for the loading-status popup.
   */
  void startAsyncImageLoad(
    const std::string& windowTitleStatus,
    std::function<bool()> loadTask,
    std::function<void()> onLoadFailed,
    bool showLoadingOverlay = true,
    std::vector<GuiData::LoadingStatusItem> loadingItems = {},
    std::string loadingStatusTitle = "Loading Images");

  /** @brief Show the loading-status popup. */
  void beginLoadingStatus(std::string title, std::vector<GuiData::LoadingStatusItem> items);

  /** @brief Mark one loading-status row as loaded. */
  void markLoadingStatusItemLoaded(GuiData::LoadingStatusItem::Kind kind, const std::filesystem::path& fileName);

  /** @brief Hide and clear the loading-status popup. */
  void hideLoadingStatus();

  /** @brief Begin loading a serialized project, including any large-image preflight prompts. */
  void beginLoadProject(serialize::EntropyProject project, std::optional<std::filesystem::path> projectFileName);

  /**
   * @brief Load image files after any unsaved-project prompt has been resolved.
   * @param fileNames Image paths to load into a new project.
   */
  void performLoadImageFiles(const std::vector<std::filesystem::path>& fileNames);

  /**
   * @brief Open DICOM inputs after any unsaved-project prompt has been resolved.
   * @param folderNames DICOM folders or files to scan.
   */
  void performOpenDicomSeriesFolders(const std::vector<std::filesystem::path>& folderNames);

  /**
   * @brief Load a project file after any unsaved-project prompt has been resolved.
   * @param fileName Project file to load.
   */
  void performLoadProjectFile(const std::filesystem::path& fileName);

  /**
   * @brief Ask before replacing a dirty project.
   * @param action Pending action to continue if the user saves or discards changes.
   * @return True when user confirmation is required.
   */
  bool requestProjectReplacement(GuiData::UnsavedProjectAction action);

  /** @brief Clear pending paths captured for a deferred project replacement. */
  void clearPendingProjectReplacement();

  /** @brief Continue project loading after a large-image prompt was answered. */
  void continueLargeImageProjectPreflight();

  /** @brief Apply the user decision from a large-image prompt. */
  void handleLargeImageLoadDecision(GuiData::LargeImageLoadDecision decision);

  /** @brief Load a serialized project snapshot into application data. */
  bool loadProject(const serialize::EntropyProject& project);

  /** @brief Create a serialized snapshot of the current project state. */
  serialize::EntropyProject createProjectSnapshot() const;

  /** @brief Create a serialized snapshot for one image and its dependent data. */
  serialize::Image createImageSnapshot(const uuids::uuid& imageUid) const;

  /** @brief Return true when current state differs from the saved project snapshot. */
  bool projectHasUnsavedChanges() const;

  /** @brief Return true when any annotation is dirty or lacks a save path. */
  bool hasUnsavedAnnotations() const;

  /** @brief Save dirty annotations, prompting for paths as needed. */
  bool saveDirtyAnnotationsWithDialogs();

  /** @brief Save annotations belonging to one image. */
  bool saveAnnotationsForImage(const uuids::uuid& imageUid, const std::filesystem::path& fileName);

  /** @brief Update the saved-project baseline snapshot after a successful save/load. */
  void markProjectSavedSnapshot();

  /** @brief Current short status text for the application window title. */
  std::string windowTitleStatus() const;

  /** @brief Apply current project/load state to the application window title. */
  void updateWindowTitleStatus();

  /** @brief Record recent image paths and persist application settings. */
  void recordRecentImageGroup(const std::vector<std::filesystem::path>& fileNames);

  /** @brief Record recent DICOM input paths and persist application settings. */
  void recordRecentDicomGroup(const std::vector<std::filesystem::path>& folderNames);

  /** @brief Record a recent project path and persist application settings. */
  void recordRecentProjectFile(const std::filesystem::path& fileName);

  /** @brief Persist application settings without marking them dirty. */
  void saveAppSettingsQuietly();

  /**
   * @brief Load an image from disk.
   * @param fileName Image path.
   * @param ignoreIfAlreadyLoaded True to return an existing image instead of reloading.
   * @return Image UID and true when loaded; existing UID and false when already loaded.
   * @throws Propagates image-loading exceptions from the image library.
   */
  std::pair<std::optional<uuids::uuid>, bool> loadImage(
    const std::filesystem::path& fileName,
    bool ignoreIfAlreadyLoaded);

  /**
   * @brief Load an already-discovered DICOM series without rescanning its source folder.
   * @param series DICOM series to load.
   * @return UID and flag if loaded.
   * @throws Propagates image-loading exceptions from the image library.
   */
  std::pair<std::optional<uuids::uuid>, bool> loadDicomSeriesImage(const dicom::SeriesInfo& series);

  /**
   * @brief Native slice view type for each loaded image that came from a DICOM series.
   * @return Map from image UID to the nearest axial, coronal, or sagittal slice orientation.
   */
  std::unordered_map<uuids::uuid, ViewType> dicomNativeViewTypesByImage() const;

  std::future<void> m_futureLoadProject;
  std::future<dicom::DiscoverResult> m_futureDiscoverDicom;
  bool m_pendingDicomScanAddToExistingProject = false;

  /**
   * DICOM source metadata keyed by loaded image UID for project serialization.
   */
  std::unordered_map<uuids::uuid, serialize::DicomSource> m_dicomSourcesByImageUid;

  /**
   * Atomic boolean that is set to true iff image loading is cancelled
   */
  std::atomic<bool> m_imageLoadCancelled;

  /**
   * Atomic boolean set to true when all project images are loaded from disk and
   * ready to be loaded into textures
   */
  std::atomic<bool> m_imagesReady;

  /**
   * Atomic boolean set to true iff images could not be loaded.
   * If true, this flag will cause the render loop to exit.
   */
  std::atomic<bool> m_imageLoadFailed;

  /**
   * True when onImagesReady is handling a live Add Image operation instead of initial load.
   */
  bool m_preserveLayoutsOnImagesReady = false;

  /**
   * Images added by the current live Add Image operation, if any.
   */
  std::vector<uuids::uuid> m_pendingAddedImageUids;
  std::optional<std::filesystem::path> m_pendingLayoutsFile = std::nullopt;

  struct PendingWarpAssignment
  {
    uuids::uuid imageUid;
    uuids::uuid warpUid;
    bool loaded = false;
    bool forwardWarp = false;
    std::optional<uuids::uuid> inverseWarpReferenceImageUid = std::nullopt;
  };

  struct PendingInverseWarpReference
  {
    uuids::uuid imageUid;
    std::filesystem::path referenceImageFileName;
  };

  std::optional<PendingWarpAssignment> m_pendingWarpAssignment = std::nullopt;
  std::vector<PendingInverseWarpReference> m_pendingInverseWarpReferences;

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
  std::vector<std::filesystem::path> m_pendingProjectReplacementPaths;

  GlfwWrapper m_glfw;                                  //!< GLFW wrapper
  AppData m_data;                                      //!< Application data
  Rendering m_rendering;                               //!< Render logic
  CallbackHandler m_callbackHandler;                   //!< UI callback handlers
  ItkSnapSync m_itkSnapSync;                           //!< Cursor, zoom, and pan synchronization with ITK-SNAP
  app_sync::EntropyInstanceSync m_entropyInstanceSync; //!< Synchronization with running Entropy instances
  ImGuiWrapper m_imgui;                                //!< ImGui wrapper
};
