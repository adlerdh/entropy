#pragma once

#include "common/AsyncTasks.h"
#include "common/CoordinateFrame.h"
#include "common/PublicTypes.h"
#include "common/Types.h"

#include "logic/camera/CameraTypes.h"
#include "windowing/ViewTypes.h"

#include "ui/UiControls.h"

#include <glm/fwd.hpp>

#include <uuid.h>

#include <functional>
#include <future>
#include <optional>
#include <utility>

class AppData;
class ImageColorMap;
class ParcellationLabelTable;

void renderViewSettingsComboWindow(
  const uuids::uuid& viewOrLayoutUid,

  const FrameBounds& mindowFrameBounds,
  const UiControls& uiControls,
  bool /*hasFrameAndBackground*/,
  bool showApplyToAllButton,

  const CoordinateFrame& worldCrosshairs,
  const glm::vec2& contentScales,

  size_t numImages,

  const std::function<bool(size_t index)>& isImageRendered,
  const std::function<void(size_t index, bool visible)>& setImageRendered,

  const std::function<bool(size_t index)>& isImageUsedForMetric,
  const std::function<void(size_t index, bool visible)>& setImageUsedForMetric,

  const std::function<std::pair<std::string, std::string>(size_t index)>& getImageDisplayAndFileName,
  const std::function<bool(size_t imageIndex)>& getImageVisibilitySetting,
  const std::function<bool(size_t imageIndex)>& getImageIsActive,

  const ViewType& viewType,
  const ViewRenderMode& renderMode,
  const IntensityProjectionMode& intensityProjMode,

  const std::function<void(const ViewType& viewType)>& setViewType,
  const std::function<void(const ViewRenderMode& renderMode)>& setRenderMode,
  const std::function<void(const IntensityProjectionMode& projMode)>& setIntensityProjectionMode,
  const std::function<void()>& recenter,

  const std::function<void(const uuids::uuid& viewUid)>& applyImageSelectionAndShaderToAllViews,

  const std::function<float()>& getIntensityProjectionSlabThickness,
  const std::function<void(float thickness)>& setIntensityProjectionSlabThickness,

  const std::function<bool()>& getDoMaxExtentIntensityProjection,
  const std::function<void(bool set)>& setDoMaxExtentIntensityProjection,

  const std::function<float()>& getXrayProjectionWindow,
  const std::function<void(float window)>& setXrayProjectionWindow,

  const std::function<float()>& getXrayProjectionLevel,
  const std::function<void(float window)>& setXrayProjectionLevel,

  const std::function<float()>& getXrayProjectionEnergy,
  const std::function<void(float window)>& setXrayProjectionEnergy);

void renderViewOrientationToolWindow(
  const uuids::uuid& viewOrLayoutUid,
  const FrameBounds& mindowFrameBounds,
  const UiControls& uiControls,
  bool hasFrameAndBackground,
  const ViewType& viewType,
  const std::function<glm::quat()>& getViewCameraRotation,
  const std::function<void(const glm::quat& camera_T_world_rotationDelta)>& setViewCameraRotation,
  const std::function<void(const glm::vec3& worldDirection)>& setViewCameraDirection,
  const std::function<glm::vec3()>& getViewNormal,
  const std::function<std::vector<glm::vec3>(const uuids::uuid& viewUidToExclude)>& getObliqueViewDirections);

void renderImagePropertiesWindow(
  AppData& appData,
  size_t numImages,
  const std::function<std::pair<std::string, std::string>(size_t index)>& getImageDisplayAndFileName,
  const std::function<size_t(void)>& getActiveImageIndex,
  const std::function<void(size_t)>& setActiveImageIndex,
  const std::function<size_t(void)>& getNumImageColorMaps,
  const std::function<ImageColorMap*(size_t cmapIndex)>& getImageColorMap,
  const std::function<bool(const uuids::uuid& imageUid)>& moveImageBackward,
  const std::function<bool(const uuids::uuid& imageUid)>& moveImageForward,
  const std::function<bool(const uuids::uuid& imageUid)>& moveImageToBack,
  const std::function<bool(const uuids::uuid& imageUid)>& moveImageToFront,
  const std::function<void(void)>& updateAllImageUniforms,
  const std::function<void(const uuids::uuid& imageUid)>& updateImageUniforms,
  const std::function<void(const uuids::uuid& imageUid)>& updateImageInterpolationMode,
  const std::function<void(std::size_t cmapIndex)>& updateImageColorMapInterpolationMode,
  const std::function<bool(const uuids::uuid& imageUid, bool locked)>& setLockManualImageTransformation,
  const AllViewsRecenterType& recenterAllViews);

void renderSegmentationPropertiesWindow(
  AppData& appData,
  const std::function<ParcellationLabelTable*(size_t tableIndex)>& getLabelTable,
  const std::function<void(const uuids::uuid& imageUid)>& updateImageUniforms,
  const std::function<void(size_t labelColorTableIndex)>& updateLabelColorTableTexture,
  const std::function<void(const uuids::uuid& imageUid, size_t labelIndex)>& moveCrosshairsToSegLabelCentroid,
  const std::function<std::optional<uuids::uuid>(
    const uuids::uuid& matchingImageUid, const std::string& segDisplayName)>& createBlankSeg,
  const std::function<bool(const uuids::uuid& segUid)>& clearSeg,
  const std::function<bool(const uuids::uuid& segUid)>& removeSeg,
  const AllViewsRecenterType& recenterAllViews);

void renderLandmarkPropertiesWindow(
  AppData& appData,
  const AllViewsRecenterType& recenterAllViews);

void renderAnnotationWindow(
  AppData& appData,
  const std::function<void(const uuids::uuid& viewUid, const glm::vec3& worldFwdDirection)>& setViewCameraDirection,
  const std::function<void()>& paintActiveSegmentationWithActivePolygon,
  const AllViewsRecenterType& recenterAllViews);

void renderIsosurfacesWindow(
  AppData& appData,
  std::function<void(const uuids::uuid& taskUid, std::future<AsyncTaskDetails> future)> storeFuture,
  std::function<void(const uuids::uuid& taskUid)> addTaskToIsosurfaceGpuMeshGenerationQueue);

void renderSettingsWindow(
  AppData& appData,
  const std::function<size_t(void)>& getNumImageColorMaps,
  const std::function<const ImageColorMap*(size_t cmapIndex)>& getImageColorMap,
  const std::function<void(void)>& updateMetricUniforms,
  const AllViewsRecenterType& recenterAllViews);

void renderInspectionWindow(
  AppData& appData,
  size_t numImages,
  const std::function<std::pair<std::string, std::string>(size_t index)>& getImageDisplayAndFileName,
  const std::function<glm::vec3()>& getWorldDeformedPos,
  const std::function<std::optional<glm::vec3>(size_t imageIndex)>& getSubjectPos,
  const std::function<std::optional<glm::ivec3>(size_t imageIndex)>& getVoxelPos,
  const std::function<std::optional<double>(size_t imageIndex)>& getImageValueNN,
  const std::function<std::optional<double>(size_t imageIndex)>& getImageValueLinear,
  const std::function<std::optional<int64_t>(size_t imageIndex)>& getSegLabel,
  const std::function<ParcellationLabelTable*(size_t tableIndex)>& getLabelTable);

void renderInspectionWindowWithTable(
  AppData& appData,
  const std::function<std::pair<std::string, std::string>(size_t index)>& getImageDisplayAndFileName,
  const std::function<std::optional<glm::vec3>(size_t imageIndex)>& getSubjectPos,
  const std::function<std::optional<glm::ivec3>(size_t imageIndex)>& getVoxelPos,
  const std::function<void(size_t imageIndex, const glm::vec3& subjectPos)> setSubjectPos,
  const std::function<void(size_t imageIndex, const glm::ivec3& voxelPos)> setVoxelPos,
  const std::function<std::vector<double>(size_t imageIndex, bool getOnlyActiveComponent)>& getImageValuesNN,
  const std::function<std::vector<double>(size_t imageIndex, bool getOnlyActiveComponent)>& getImageValuesLinear,
  const std::function<std::optional<int64_t>(size_t imageIndex)>& getSegLabel,
  const std::function<ParcellationLabelTable*(size_t tableIndex)>& getLabelTable);

void renderOpacityBlenderWindow(
  AppData& appData,
  const std::function<void(const uuids::uuid& imageUid)>& updateImageUniforms);
