#pragma once

#include "common/AsyncTasks.h"
#include <filesystem>
#include "common/PublicTypes.h"
#include "image/DicomSeries.h"
#include "common/SegmentationTypes.h"
#include "ui/GuiData.h"
#include "ui/UiScaleManager.h"

#include <glm/fwd.hpp>
#include <uuid.h>

#include <functional>
#include <future>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

class AppData;
class CallbackHandler;
struct GLFWwindow;

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

  void setCallbacks(
    std::function<void(void)> postEmptyGlfwEvent,
    std::function<void(void)> readjustViewport,
    std::function<void(const std::vector<std::filesystem::path>& fileNames)> openImageFiles,
    std::function<void(const std::vector<std::filesystem::path>& fileNames)> addImageFiles,
    std::function<void(const std::vector<std::filesystem::path>& folderNames)> openDicomFolders,
    std::function<void(const std::filesystem::path& fileName)> addSegmentationFile,
    std::function<void(const uuids::uuid& imageUid, const std::filesystem::path& fileName)> addSegmentationFileToImage,
    std::function<void(const std::filesystem::path& fileName)> openProjectFile,
    std::function<void(GuiData::LargeImageLoadDecision decision)> largeImageLoadDecision,
    std::function<void(
      const std::vector<dicom::SeriesInfo>& series,
      std::optional<std::size_t> referenceSeriesIndex,
      bool addToExistingProject)> loadDicomSeries,
    std::function<bool()> saveProject,
    std::function<bool(const std::filesystem::path& fileName)> saveProjectAs,
    std::function<void()> closeProject,
    std::function<void(const std::filesystem::path& fileName)> loadLayoutsFile,
    std::function<bool(const std::filesystem::path& fileName)> saveLayoutsFile,
    std::function<void()> closeProjectWithoutPrompt,
    std::function<void()> requestQuitApp,
    std::function<void()> quitAppWithoutPrompt,
    std::function<void(const uuids::uuid& viewUid)> recenterView,
    AllViewsRecenterType recenterCurrentViews,
    std::function<bool(void)> getOverlayVisibility,
    std::function<void(bool)> setOverlayVisibility,
    std::function<void(void)> updateAllImageUniforms,
    std::function<void(const uuids::uuid& imageUid)> updateImageUniforms,
    std::function<void(const uuids::uuid& imageUid)> updateImageInterpolationMode,
    std::function<void(std::size_t cmapUid)> updateImageColorMapInterpolationMode,
    std::function<void(std::size_t tableIndex)> updateLabelColorTableTexture,
    std::function<void(const uuids::uuid& imageUid, std::size_t labelIndex)> moveCrosshairsToSegLabelCentroid,
    std::function<void()> updateMetricUniforms,
    std::function<glm::vec3()> getWorldDeformedPos,
    std::function<std::optional<glm::vec3>(std::size_t imageIndex)> getSubjectPos,
    std::function<std::optional<glm::ivec3>(std::size_t imageIndex)> getVoxelPos,
    std::function<void(std::size_t imageIndex, const glm::vec3& subjectPos)> setSubjectPos,
    std::function<void(std::size_t imageIndex, const glm::ivec3& voxelPos)> setVoxelPos,
    std::function<std::vector<double>(std::size_t imageIndex, bool getOnlyActiveComponent)> getImageValuesNN,
    std::function<std::vector<double>(std::size_t imageIndex, bool getOnlyActiveComponent)> getImageValuesLinear,
    std::function<std::optional<int64_t>(std::size_t imageIndex)> getSegLabel,
    std::function<std::optional<uuids::uuid>(const uuids::uuid& matchingImageUid, const std::string& segDisplayName)>
      createBlankSeg,
    std::function<bool(const uuids::uuid& segUid)> clearSeg,
    std::function<bool(const uuids::uuid& segUid)> removeSeg,
    std::function<bool(const uuids::uuid& imageUid, const uuids::uuid& seedSegUid, const SeedSegmentationType& segType)>
      executePoissonSeg,
    std::function<bool(const uuids::uuid& imageUid, bool locked)> setLockManualImageTransformation,
    std::function<bool(const uuids::uuid& imageUid)> setReferenceImage,
    std::function<bool(const uuids::uuid& imageUid)> removeImage,
    std::function<void()> paintActiveSegmentationWithActivePolygon);

  void render();

private:
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

  /// Futures created by running tasks asynchronously from the UI.
  /// These are created during the lifetime of the application.
  /// -Key: UID for the task
  std::unordered_map<uuids::uuid, std::future<AsyncTaskDetails> > m_futures;

  /// Mutex protecting \c m_futures
  std::mutex m_futuresMutex;

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
