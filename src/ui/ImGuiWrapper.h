#ifndef IMGUI_WRAPPER_H
#define IMGUI_WRAPPER_H

#include "common/AsyncTasks.h"
#include "common/PublicTypes.h"
#include "common/SegmentationTypes.h"

#include <glm/fwd.hpp>
#include <uuid.h>

#include <functional>
#include <future>
#include <mutex>
#include <queue>
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

  void setContentScale(float scale);

  void setCallbacks(
    std::function<void(void)> postEmptyGlfwEvent,
    std::function<void(void)> readjustViewport,
    std::function<void(const uuids::uuid& viewUid)> recenterView,
    AllViewsRecenterType recenterCurrentViews,
    std::function<bool(void)> getOverlayVisibility,
    std::function<void(bool)> setOverlayVisibility,
    std::function<void(void)> updateAllImageUniforms,
    std::function<void(const uuids::uuid& viewUid)> updateImageUniforms,
    std::function<void(const uuids::uuid& imageUid)> updateImageInterpolationMode,
    std::function<void(std::size_t cmapUid)> updateImageColorMapInterpolationMode,
    std::function<void(std::size_t tableIndex)> updateLabelColorTableTexture,
    std::function<void(const uuids::uuid& imageUid, std::size_t labelIndex)>
      moveCrosshairsToSegLabelCentroid,
    std::function<void()> updateMetricUniforms,
    std::function<glm::vec3()> getWorldDeformedPos,
    std::function<std::optional<glm::vec3>(std::size_t imageIndex)> getSubjectPos,
    std::function<std::optional<glm::ivec3>(std::size_t imageIndex)> getVoxelPos,
    std::function<void(std::size_t imageIndex, const glm::vec3& subjectPos)> setSubjectPos,
    std::function<void(std::size_t imageIndex, const glm::ivec3& voxelPos)> setVoxelPos,
    std::function<std::vector<double>(std::size_t imageIndex, bool getOnlyActiveComponent)>
      getImageValuesNN,
    std::function<std::vector<double>(std::size_t imageIndex, bool getOnlyActiveComponent)>
      getImageValuesLinear,
    std::function<std::optional<int64_t>(std::size_t imageIndex)> getSegLabel,
    std::function<std::optional<uuids::uuid>(
      const uuids::uuid& matchingImageUid, const std::string& segDisplayName
    )> createBlankSeg,
    std::function<bool(const uuids::uuid& segUid)> clearSeg,
    std::function<bool(const uuids::uuid& segUid)> removeSeg,
    std::function<bool(
      const uuids::uuid& imageUid, const uuids::uuid& seedSegUid, const SeedSegmentationType& segType
    )> executeGraphCutsSeg,
    std::function<bool(
      const uuids::uuid& imageUid, const uuids::uuid& seedSegUid, const SeedSegmentationType& segType
    )> executePoissonSeg,
    std::function<bool(const uuids::uuid& imageUid, bool locked)> setLockManualImageTransformation,
    std::function<void()> paintActiveSegmentationWithActivePolygon
  );

  void render();

private:
  void initializeFonts();

  void annotationToolbar(const std::function<void()> paintActiveAnnotation);

  AppData& m_appData;
  CallbackHandler& m_callbackHandler;

  // Callbacks:
  std::function<void(void)> m_postEmptyGlfwEvent = nullptr;
  std::function<void(void)> m_readjustViewport = nullptr;
  std::function<void(const uuids::uuid& viewUid)> m_recenterView = nullptr;
  AllViewsRecenterType m_recenterAllViews = nullptr;
  std::function<bool(void)> m_getOverlayVisibility = nullptr;
  std::function<void(bool)> m_setOverlayVisibility = nullptr;
  std::function<void(void)> m_updateAllImageUniforms = nullptr;
  std::function<void(const uuids::uuid& viewUid)> m_updateImageUniforms = nullptr;
  std::function<void(const uuids::uuid& viewUid)> m_updateImageInterpolationMode = nullptr;
  std::function<void(std::size_t cmapIndex)> m_updateImageColorMapInterpolationMode = nullptr;
  std::function<void(std::size_t tableIndex)> m_updateLabelColorTableTexture = nullptr;
  std::function<void(const uuids::uuid& imageUid, std::size_t labelIndex)>
    m_moveCrosshairsToSegLabelCentroid = nullptr;
  std::function<void(void)> m_updateMetricUniforms = nullptr;
  std::function<glm::vec3()> m_getWorldDeformedPos = nullptr;
  std::function<std::optional<glm::vec3>(std::size_t imageIndex)> m_getSubjectPos = nullptr;
  std::function<std::optional<glm::ivec3>(std::size_t imageIndex)> m_getVoxelPos = nullptr;
  std::function<void(std::size_t imageIndex, const glm::vec3& subjectPos)> m_setSubjectPos = nullptr;
  std::function<void(std::size_t imageIndex, const glm::ivec3& voxelPos)> m_setVoxelPos = nullptr;
  std::function<std::vector<double>(std::size_t imageIndex, bool getOnlyActiveComponent)>
    m_getImageValuesNN = nullptr;
  std::function<std::vector<double>(std::size_t imageIndex, bool getOnlyActiveComponent)>
    m_getImageValuesLinear = nullptr;
  std::function<std::optional<int64_t>(std::size_t imageIndex)> m_getSegLabel = nullptr;
  std::function<std::optional<
    uuids::uuid>(const uuids::uuid& matchingImageUid, const std::string& segDisplayName)>
    m_createBlankSeg = nullptr;
  std::function<bool(const uuids::uuid& segUid)> m_clearSeg = nullptr;
  std::function<bool(const uuids::uuid& segUid)> m_removeSeg = nullptr;
  std::function<
    bool(const uuids::uuid& imageUid, const uuids::uuid& seedSegUid, const SeedSegmentationType&)>
    m_executeGraphCutsSeg = nullptr;
  std::function<
    bool(const uuids::uuid& imageUid, const uuids::uuid& seedSegUid, const SeedSegmentationType&)>
    m_executePoissonSeg = nullptr;
  std::function<bool(const uuids::uuid& imageUid, bool locked)> m_setLockManualImageTransformation
    = nullptr;
  std::function<void()> m_paintActiveSegmentationWithActivePolygon = nullptr;

  /// Scaling for the UI elements and fonts
  float m_contentScale;

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

  std::pair<const char*, const char*> getImageDisplayAndFileNames(std::size_t imageIndex) const;
};

#endif // IMGUI_WRAPPER_H
