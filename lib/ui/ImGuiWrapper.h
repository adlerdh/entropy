#pragma once

#include "common/AsyncTasks.h"
#include "common/PublicTypes.h"
#include "common/SegmentationTypes.h"
#include "image/DicomSeries.h"
#include "image/ImageDerivedData.h"
#include "image/WarpInversion.h"
#include "logic/app/Settings.h"
#include "registration/Execution.h"
#include "ui/GuiData.h"
#include "ui/UiScaleManager.h"
#include "ui/updates/UpdateCheck.h"

#include <glm/fwd.hpp>
#include <uuid.h>

#include <cstdint>
#include <atomic>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class AppData;
class CallbackHandler;
struct GLFWwindow;

/**
 * @brief Platform/window callbacks required by the ImGui layer.
 */
struct ImGuiPlatformCallbacks
{
  /** @brief Wake the GLFW event loop after UI work schedules a redraw. */
  std::function<void()> postEmptyGlfwEvent;

  /** @brief Resize and re-layout the viewport after UI state changes. */
  std::function<void()> readjustViewport;
};

/**
 * @brief Project and file callbacks owned by the application layer.
 */
struct ImGuiProjectCallbacks
{
  /** @brief Open image files as a new project. */
  std::function<void(const std::vector<std::filesystem::path>& fileNames)> openImageFiles;

  /** @brief Add image files to the current project. */
  std::function<void(const std::vector<std::filesystem::path>& fileNames)> addImageFiles;

  /** @brief Open DICOM folders and scan them for series. */
  std::function<void(const std::vector<std::filesystem::path>& folderNames)> openDicomFolders;

  /** @brief Add a segmentation file to the active image. */
  std::function<void(const std::filesystem::path& fileName)> addSegmentationFile;

  /** @brief Add a segmentation file to a specific image. */
  std::function<void(const uuids::uuid& imageUid, const std::filesystem::path& fileName)> addSegmentationFileToImage;

  /** @brief Load a warp field and return its UID if successful. */
  std::function<std::optional<uuids::uuid>(const std::filesystem::path& fileName)> loadDeformationField;

  /** @brief Import completed registration outputs through the application loading pipeline. */
  std::function<void(const std::string& jobId)> importRegistrationJobOutputs;

  /** @brief Open a project file. */
  std::function<void(const std::filesystem::path& fileName)> openProjectFile;

  /** @brief Resolve a pending large-image load prompt. */
  std::function<void(GuiData::LargeImageLoadDecision decision)> largeImageLoadDecision;

  /** @brief Load selected DICOM series into a new or existing project. */
  std::function<void(
    const std::vector<dicom::SeriesInfo>& series,
    std::optional<std::size_t> referenceSeriesIndex,
    bool addToExistingProject)>
    loadDicomSeries;

  /** @brief Save the current project. */
  std::function<bool()> saveProject;

  /** @brief Save the current project to a selected path. */
  std::function<bool(const std::filesystem::path& fileName)> saveProjectAs;

  /** @brief Prompt to close the current project. */
  std::function<void()> closeProject;

  /** @brief Load a layout definition file. */
  std::function<void(const std::filesystem::path& fileName)> loadLayoutsFile;

  /** @brief Save layouts to a selected path. */
  std::function<bool(const std::filesystem::path& fileName)> saveLayoutsFile;

  /** @brief Reset project-wide settings without removing loaded data. */
  std::function<void()> resetProjectSettings;

  /** @brief Close the current project without prompting. */
  std::function<void()> closeProjectWithoutPrompt;

  /** @brief Prompt to quit the application. */
  std::function<void()> requestQuitApp;

  /** @brief Quit the application without prompting. */
  std::function<void()> quitAppWithoutPrompt;
};

/**
 * @brief View, overlay, and rendering callbacks invoked from UI controls.
 */
struct ImGuiViewCallbacks
{
  /** @brief Recenter a specific view. */
  std::function<void(const uuids::uuid& viewUid)> recenterView;

  /** @brief Recenter the current layout's views with selectable reset behavior. */
  AllViewsRecenterType recenterCurrentViews;

  /** @brief Query whether view overlays are visible. */
  std::function<bool()> getOverlayVisibility;

  /** @brief Set whether view overlays are visible. */
  std::function<void(bool)> setOverlayVisibility;

  /** @brief Update uniforms for all image renderers. */
  std::function<void()> updateAllImageUniforms;

  /** @brief Update uniforms for one image renderer. */
  std::function<void(const uuids::uuid& imageUid)> updateImageUniforms;

  /** @brief Update interpolation state for one image renderer. */
  std::function<void(const uuids::uuid& imageUid)> updateImageInterpolationMode;

  /** @brief Update interpolation state for one image colormap texture. */
  std::function<void(std::size_t cmapIndex)> updateImageColorMapInterpolationMode;

  /** @brief Update one segmentation label color table texture. */
  std::function<void(std::size_t tableIndex)> updateLabelColorTableTexture;

  /** @brief Move crosshairs to the centroid of a segmentation label. */
  std::function<void(const uuids::uuid& imageUid, std::size_t labelIndex)> moveCrosshairsToSegLabelCentroid;

  /** @brief Update metric shader uniforms after measurement settings change. */
  std::function<void()> updateMetricUniforms;
};

/**
 * @brief Crosshairs and voxel inspection callbacks used by status/UI widgets.
 */
struct ImGuiInspectionCallbacks
{
  /** @brief Get the current deformed world-space crosshairs position. */
  std::function<glm::vec3()> getWorldDeformedPos;

  /** @brief Get crosshairs position in image subject coordinates. */
  std::function<std::optional<glm::vec3>(std::size_t imageIndex)> getSubjectPos;

  /** @brief Get crosshairs position in image voxel coordinates. */
  std::function<std::optional<glm::ivec3>(std::size_t imageIndex)> getVoxelPos;

  /** @brief Set crosshairs from image subject coordinates. */
  std::function<void(std::size_t imageIndex, const glm::vec3& subjectPos)> setSubjectPos;

  /** @brief Set crosshairs from image voxel coordinates. */
  std::function<void(std::size_t imageIndex, const glm::ivec3& voxelPos)> setVoxelPos;

  /** @brief Get nearest-neighbor image values at the crosshairs. */
  std::function<std::vector<double>(std::size_t imageIndex, bool getOnlyActiveComponent)> getImageValuesNN;

  /** @brief Get linearly interpolated image values at the crosshairs. */
  std::function<std::vector<double>(std::size_t imageIndex, bool getOnlyActiveComponent)> getImageValuesLinear;

  /** @brief Get the active segmentation label at the crosshairs. */
  std::function<std::optional<int64_t>(std::size_t imageIndex)> getSegLabel;
};

/**
 * @brief Segmentation and image-management callbacks invoked from UI controls.
 */
struct ImGuiEditingCallbacks
{
  /** @brief Create a blank segmentation for an image. */
  std::function<std::optional<uuids::uuid>(const uuids::uuid& matchingImageUid, const std::string& segDisplayName)>
    createBlankSeg;

  /** @brief Clear all voxels in a segmentation. */
  std::function<bool(const uuids::uuid& segUid)> clearSeg;

  /** @brief Remove a segmentation. */
  std::function<bool(const uuids::uuid& segUid)> removeSeg;

  /** @brief Execute seed-based Poisson segmentation. */
  std::function<bool(const uuids::uuid& imageUid, const uuids::uuid& seedSegUid, const SeedSegmentationType& segType)>
    executePoissonSeg;

  /** @brief Lock or unlock an image's manual transformation. */
  std::function<bool(const uuids::uuid& imageUid, bool locked)> setLockManualImageTransformation;

  /** @brief Make an image the project reference image. */
  std::function<bool(const uuids::uuid& imageUid)> setReferenceImage;

  /** @brief Remove an image from the project. */
  std::function<bool(const uuids::uuid& imageUid)> removeImage;

  /** @brief Paint the active segmentation from the active annotation. */
  std::function<void()> paintActiveSegmentationWithActivePolygon;
};

/**
 * @brief Complete set of application callbacks required by ImGuiWrapper.
 */
struct ImGuiWrapperCallbacks
{
  /** @brief Platform/window callbacks. */
  ImGuiPlatformCallbacks platform;

  /** @brief Project and file callbacks. */
  ImGuiProjectCallbacks project;

  /** @brief View and rendering callbacks. */
  ImGuiViewCallbacks view;

  /** @brief Crosshairs and voxel inspection callbacks. */
  ImGuiInspectionCallbacks inspection;

  /** @brief Segmentation and image-editing callbacks. */
  ImGuiEditingCallbacks editing;
};

/**
 * @brief A simple wrapper for ImGui
 */
class ImGuiWrapper
{
public:
  ImGuiWrapper(GLFWwindow* window, AppData& appData, CallbackHandler& callbackHandler);
  ~ImGuiWrapper();

  /**
   * @brief Apply a new platform content scale reported by the windowing layer.
   *
   * This is the monitor/window content scale from GLFW. The scale manager maps
   * it to the UI scale that should be used on the current platform.
   */
  void setContentScale(float scale);

  /**
   * @brief Queue a user-selected ImGui UI scale override.
   *
   * Font atlas changes are deferred until the next render frame so that they
   * happen before ImGui::NewFrame().
   */
  void setUserScaleOverride(std::optional<float> scale);

  /**
   * @brief Queue a reload of the ImGui font atlas using the active UI font.
   */
  void requestFontReload();

  /**
   *  Apply a UI color preset and keep it as the scale baseline.
   */
  void applyUiColorPreset(UiColorPreset preset);

  /**
   *  Apply a UI density preset and keep it as the scale baseline.
   */
  void applyUiDensityPreset(UiDensityPreset preset);

  /**
   *  Apply regular window background opacity and keep it as the scale baseline.
   */
  void applyUiWindowBgOpacity(float opacity);

  /**
   * @brief Install application callbacks used by the ImGui layer.
   *
   * @param callbacks Grouped callback object. Grouping keeps the API readable
   * and avoids errors from wiring a long positional parameter list.
   */
  void setCallbacks(ImGuiWrapperCallbacks callbacks);

  void render();

private:
  bool materializeRegistrationInputs(registration::JobSpec& job);

  /**
   * @brief Load the active ImGui UI font and icon font into the font atlas.
   *
   * The font sizes are base logical sizes. DPI scaling is applied separately
   * through ImGuiStyle::FontScaleDpi by UiScaleManager.
   */
  void initializeFonts(float scale);

  void annotationToolbar(const std::function<void()> paintActiveAnnotation);

  AppData& m_appData;
  CallbackHandler& m_callbackHandler;
  GLFWwindow* m_window;
  std::filesystem::path m_iniFilePath;
  std::filesystem::path m_logFilePath;
  std::string m_iniFileName;
  std::string m_logFileName;

  // Callbacks:
  std::function<void(void)> m_postEmptyGlfwEvent = nullptr;
  std::function<void(void)> m_readjustViewport = nullptr;
  std::function<void(const std::vector<std::filesystem::path>& fileNames)> m_openImageFiles = nullptr;
  std::function<void(const std::vector<std::filesystem::path>& fileNames)> m_addImageFiles = nullptr;
  std::function<void(const std::vector<std::filesystem::path>& folderNames)> m_openDicomFolders = nullptr;
  std::function<void(const std::filesystem::path& fileName)> m_addSegmentationFile = nullptr;
  std::function<void(const uuids::uuid& imageUid, const std::filesystem::path& fileName)> m_addSegmentationFileToImage =
    nullptr;
  std::function<std::optional<uuids::uuid>(const std::filesystem::path& fileName)> m_loadDeformationField = nullptr;
  std::function<void(const std::string& jobId)> m_importRegistrationJobOutputs = nullptr;
  std::function<void(const std::filesystem::path& fileName)> m_openProjectFile = nullptr;
  std::function<void(GuiData::LargeImageLoadDecision decision)> m_largeImageLoadDecision = nullptr;
  std::function<void(
    const std::vector<dicom::SeriesInfo>& series,
    std::optional<std::size_t> referenceSeriesIndex,
    bool addToExistingProject)>
    m_loadDicomSeries = nullptr;
  std::function<bool()> m_saveProject = nullptr;
  std::function<bool(const std::filesystem::path& fileName)> m_saveProjectAs = nullptr;
  std::function<void()> m_closeProject = nullptr;
  std::function<void(const std::filesystem::path& fileName)> m_loadLayoutsFile = nullptr;
  std::function<bool(const std::filesystem::path& fileName)> m_saveLayoutsFile = nullptr;
  std::function<void()> m_resetProjectSettings = nullptr;
  std::function<void()> m_closeProjectWithoutPrompt = nullptr;
  std::function<void()> m_requestQuitApp = nullptr;
  std::function<void()> m_quitAppWithoutPrompt = nullptr;
  std::function<void(const uuids::uuid& viewUid)> m_recenterView = nullptr;
  AllViewsRecenterType m_recenterAllViews = nullptr;
  std::function<bool(void)> m_getOverlayVisibility = nullptr;
  std::function<void(bool)> m_setOverlayVisibility = nullptr;
  std::function<void(void)> m_updateAllImageUniforms = nullptr;
  std::function<void(const uuids::uuid& imageUid)> m_updateImageUniforms = nullptr;
  std::function<void(const uuids::uuid& imageUid)> m_updateImageInterpolationMode = nullptr;
  std::function<void(std::size_t cmapIndex)> m_updateImageColorMapInterpolationMode = nullptr;
  std::function<void(std::size_t tableIndex)> m_updateLabelColorTableTexture = nullptr;
  std::function<void(const uuids::uuid& imageUid, std::size_t labelIndex)> m_moveCrosshairsToSegLabelCentroid = nullptr;
  std::function<void(void)> m_updateMetricUniforms = nullptr;
  std::function<glm::vec3()> m_getWorldDeformedPos = nullptr;
  std::function<std::optional<glm::vec3>(std::size_t imageIndex)> m_getSubjectPos = nullptr;
  std::function<std::optional<glm::ivec3>(std::size_t imageIndex)> m_getVoxelPos = nullptr;
  std::function<void(std::size_t imageIndex, const glm::vec3& subjectPos)> m_setSubjectPos = nullptr;
  std::function<void(std::size_t imageIndex, const glm::ivec3& voxelPos)> m_setVoxelPos = nullptr;
  std::function<std::vector<double>(std::size_t imageIndex, bool getOnlyActiveComponent)> m_getImageValuesNN = nullptr;
  std::function<std::vector<double>(std::size_t imageIndex, bool getOnlyActiveComponent)> m_getImageValuesLinear =
    nullptr;
  std::function<std::optional<int64_t>(std::size_t imageIndex)> m_getSegLabel = nullptr;
  std::function<std::optional<uuids::uuid>(const uuids::uuid& matchingImageUid, const std::string& segDisplayName)>
    m_createBlankSeg = nullptr;
  std::function<bool(const uuids::uuid& segUid)> m_clearSeg = nullptr;
  std::function<bool(const uuids::uuid& segUid)> m_removeSeg = nullptr;
  std::function<bool(const uuids::uuid& imageUid, const uuids::uuid& seedSegUid, const SeedSegmentationType&)>
    m_executePoissonSeg = nullptr;
  std::function<bool(const uuids::uuid& imageUid, bool locked)> m_setLockManualImageTransformation = nullptr;
  std::function<bool(const uuids::uuid& imageUid)> m_setReferenceImage = nullptr;
  std::function<bool(const uuids::uuid& imageUid)> m_removeImage = nullptr;
  std::function<void()> m_paintActiveSegmentationWithActivePolygon = nullptr;

  UiScaleManager m_uiScaleManager;
  std::optional<std::optional<float> > m_pendingUserScaleOverride;
  bool m_pendingFontReload = false;
  bool m_applyDefaultPanelLayout = false;

  /// Futures created by running tasks asynchronously from the UI.
  /// These are created during the lifetime of the application.
  /// -Key: UID for the task
  std::unordered_map<uuids::uuid, std::future<AsyncTaskDetails> > m_futures;

  /// Mutex protecting \c m_futures
  std::mutex m_futuresMutex;

  struct ComponentProjectionTaskResult
  {
    uuids::uuid sourceImageUid;
    ComponentProjectionMode mode{ComponentProjectionMode::Mean};
    uint32_t timePoint{0};
    entropy_expected::expected<Image, std::string> image;
  };

  std::unordered_map<uuids::uuid, std::future<ComponentProjectionTaskResult> > m_componentProjectionFutures;
  std::unordered_set<std::string> m_pendingComponentProjectionKeys;
  std::mutex m_componentProjectionFuturesMutex;

  void requestComponentProjectionImage(const uuids::uuid& imageUid, ComponentProjectionMode mode);
  void requestMissingComponentProjectionImages();
  void processComponentProjectionFutures();

  struct WarpInversionTaskState
  {
    uuids::uuid imageUid;
    uuids::uuid sourceWarpUid;
    ComputedWarpDirection direction{ComputedWarpDirection::Inverse};
    std::string description;
    std::shared_ptr<std::atomic<double> > progress;
    std::shared_ptr<std::atomic_bool> cancel;
  };

  struct WarpInversionTaskResult
  {
    uuids::uuid imageUid;
    uuids::uuid sourceWarpUid;
    ComputedWarpDirection direction{ComputedWarpDirection::Inverse};
    entropy_expected::expected<WarpInversionResult, std::string> result;
  };

  std::unordered_map<uuids::uuid, WarpInversionTaskState> m_warpInversionTaskStates;
  std::unordered_map<uuids::uuid, std::future<WarpInversionTaskResult> > m_warpInversionFutures;
  std::unordered_set<std::string> m_pendingWarpInversionKeys;
  std::mutex m_warpInversionFuturesMutex;

  void requestWarpInversion(
    const uuids::uuid& imageUid,
    const uuids::uuid& sourceWarpUid,
    ComputedWarpDirection direction,
    const WarpInversionOptions& options);
  void processWarpInversionFutures();
  void renderWarpInversionProgressPopup();

  struct RegistrationJobTaskResult
  {
    std::string jobId;
    registration::JobExecution execution;
  };

  struct PendingRegistrationOutputLine
  {
    std::string jobId;
    registration::ProcessOutputLine line;
  };

  std::unordered_map<uuids::uuid, std::future<RegistrationJobTaskResult> > m_registrationJobFutures;
  std::unordered_map<uuids::uuid, std::string> m_registrationJobIdsByTask;
  std::unordered_map<std::string, std::shared_ptr<std::atomic_bool> > m_registrationJobCancelFlags;
  std::unordered_set<std::string> m_runningRegistrationJobIds;
  std::vector<PendingRegistrationOutputLine> m_pendingRegistrationOutputLines;
  std::mutex m_registrationJobFuturesMutex;

  void requestQueuedRegistrationJobs();
  void processRegistrationJobFutures();

  std::future<entropy::ui::updates::CheckResult> m_updateCheckFuture;
  entropy::ui::updates::CheckWindowState m_updateCheckWindowState;
  std::string m_updateCheckEtag;
  bool m_automaticUpdateCheckRequested = false;

  void requestUpdateCheck(bool manualCheck);
  void requestAutomaticUpdateCheckIfNeeded();
  void processUpdateCheckFuture();

  /// Queue of UIDs referring to task UIDs of futures.
  /// These are completed isosurface mesh generation tasks that now need
  /// mesh generation to be run on the GPU.
  std::queue<uuids::uuid> m_isosurfaceTaskQueueForGpuMeshGeneration;

  /// Mutex protecting \c m_isosurfaceTaskQueueForGpuMeshGeneration
  std::mutex m_isosurfaceTaskQueueMutex;

  /// Update \c m_isosurfaceTaskQueueForGpuMeshGeneration with a new task UID.
  /// This is called once CPU mesh generation is complete.
  void addTaskToIsosurfaceGpuMeshGenerationQueue(const uuids::uuid& taskUid);

  /// Generate GPU mesh records for isosurfaces in \c m_isosurfaceTaskQueueForGpuMeshGeneration
  void generateIsosurfaceMeshGpuRecords();

  /**
   * @brief Store futures from UI tasks in \c m_futures map. Futures need to be stored so that their
   * destructors are not called. Calling the destructor of a future causes us to wait on the it.
   *
   * @param taskUid UID of the task
   * @param future The future
   */
  void storeFuture(const uuids::uuid& taskUid, std::future<AsyncTaskDetails> future);

  std::pair<std::string, std::string> getImageDisplayAndFileNames(std::size_t imageIndex) const;
};
