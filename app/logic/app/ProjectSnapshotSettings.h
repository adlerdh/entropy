#pragma once

#include "image/ImageSettings.h"
#include "logic/serialization/ProjectSerialization.h"

#include <optional>
#include <functional>

class AppData;
class Image;

namespace project_snapshot
{
/// Restore a segmentation's active assignment and optionally its display settings from a matching saved path.
void restoreSegmentationState(
  AppData& data,
  const uuids::uuid& imageUid,
  const uuids::uuid& segUid,
  const serialize::Image& record,
  bool applySettings);

/// Report a load failure using a description, optional source path, and diagnostic message.
using ImageLoadFailure =
  std::function<void(const std::string&, const std::optional<std::filesystem::path>&, const std::string&)>;

/// Apply persisted display, geometry overrides, and enabled/disabled affine state.
/// Shared by the interactive loader and headless project workflow tests.
void restoreImageState(
  Image& target,
  const serialize::Image& serializedImage,
  bool isReferenceImage,
  const ImageLoadFailure& reportFailure);

/**
 * @name Per-asset settings
 *
 * These helpers serialize settings that belong to an individual image or segmentation. They do
 * not reset or apply project-wide presentation defaults.
 */
/// @{

/**
 * @brief Build serialized image settings from the runtime image state.
 * @param image Image whose display settings should be serialized.
 * @param defaultBorderColor Optional image-order default color used to omit unchanged generated colors.
 * @return Project image settings suitable for project JSON serialization.
 */
serialize::ImageSettings imageSettings(const Image& image, std::optional<glm::vec3> defaultBorderColor = std::nullopt);

/**
 * @brief Apply serialized image settings to a loaded image.
 * @param image Image to update.
 * @param settings Serialized settings to restore.
 */
void applyImageSettings(Image& image, const serialize::ImageSettings& settings);

/**
 * @brief Build serialized segmentation settings from the runtime segmentation state.
 * @param appData Application data that owns the segmentation label tables.
 * @param segmentation Segmentation image whose settings should be serialized.
 * @return Project segmentation settings suitable for project JSON serialization.
 */
serialize::SegSettings segmentationSettings(const AppData& appData, const Image& seg);

/**
 * @brief Apply serialized segmentation settings to a loaded segmentation.
 * @param appData Application data that owns the segmentation label tables.
 * @param segmentation Segmentation image to update.
 * @param settings Serialized settings to restore.
 */
void applySegmentationSettings(AppData& appData, Image& seg, const serialize::SegSettings& settings);

/// @}

/**
 * @name Project-wide settings
 *
 * These helpers snapshot the live presentation of the current project. Some values also have a separate
 * application-default value for newly opened data; loading a project changes only the live value and never
 * promotes it into the application-default profile.
 */
/// @{

/**
 * @brief Build project-owned interface settings from the current application data.
 * @param appData Application data containing the current project review state.
 * @return Project interface settings suitable for project JSON serialization.
 */
serialize::ProjectSynchronizationSettings synchronizationSettings(const AppData& appData);

/**
 * @brief Apply project-owned interface settings to the application data.
 * @param appData Application data to update.
 * @param settings Serialized interface settings to restore.
 */
void applySynchronizationSettings(AppData& appData, const serialize::ProjectSynchronizationSettings& settings);

/**
 * @brief Build project-owned view settings from the current application data.
 * @param appData Application data containing the current project view state.
 * @return Project view settings suitable for project JSON serialization.
 */
serialize::ProjectViewSettings viewSettings(const AppData& appData);

/**
 * @brief Apply project-owned view settings to the application data.
 * @param appData Application data to update.
 * @param settings Serialized project view settings to restore.
 */
void applyViewSettings(AppData& appData, const serialize::ProjectViewSettings& settings);

/**
 * @brief Build project-owned comparison settings from the current application data.
 * @param appData Application data containing the current project comparison state.
 * @return Project comparison settings suitable for project JSON serialization.
 */
serialize::ProjectComparisonSettings comparisonSettings(const AppData& appData);

/**
 * @brief Apply project-owned comparison settings to the application data.
 * @param appData Application data to update.
 * @param settings Serialized project comparison settings to restore.
 */
void applyComparisonSettings(AppData& appData, const serialize::ProjectComparisonSettings& settings);

/**
 * @brief Build project-owned shared 3D rendering settings from the current application data.
 * @param appData Application data containing current project rendering state.
 * @return Project 3D rendering settings suitable for project JSON serialization.
 */
serialize::ProjectThreeDRenderingSettings threeDRenderingSettings(const AppData& appData);

/**
 * @brief Apply project-owned shared 3D rendering settings to the application data.
 * @param appData Application data to update.
 * @param settings Serialized 3D rendering settings to restore.
 */
void applyThreeDRenderingSettings(AppData& appData, const serialize::ProjectThreeDRenderingSettings& settings);

/**
 * @brief Build project-owned raycasting settings from the current application data.
 * @param appData Application data containing current project rendering state.
 * @return Project raycasting settings suitable for project JSON serialization.
 */
serialize::ProjectRaycastingSettings raycastingSettings(const AppData& appData);

/**
 * @brief Apply project-owned raycasting settings to the application data.
 * @param appData Application data to update.
 * @param settings Serialized raycasting settings to restore.
 */
void applyRaycastingSettings(AppData& appData, const serialize::ProjectRaycastingSettings& settings);

/**
 * @brief Build project-owned mesh rendering settings from the current application data.
 * @param appData Application data containing current project rendering state.
 * @return Project mesh rendering settings suitable for project JSON serialization.
 */
serialize::ProjectMeshRenderingSettings meshRenderingSettings(const AppData& appData);

/**
 * @brief Apply project-owned mesh rendering settings to the application data.
 * @param appData Application data to update.
 * @param settings Serialized mesh rendering settings to restore.
 */
void applyMeshRenderingSettings(AppData& appData, const serialize::ProjectMeshRenderingSettings& settings);

/**
 * @brief Build project-owned intensity projection defaults from the current application data.
 * @param appData Application data containing current project rendering state.
 * @return Project intensity projection defaults suitable for project JSON serialization.
 */
serialize::ProjectIntensityProjectionSettings intensityProjectionSettings(const AppData& appData);

/**
 * @brief Apply project-owned intensity projection defaults to the application data.
 * @param appData Application data to update.
 * @param settings Serialized intensity projection defaults to restore.
 */
void applyIntensityProjectionSettings(AppData& appData, const serialize::ProjectIntensityProjectionSettings& settings);

/**
 * @brief Build project-owned segmentation display settings from the current application data.
 * @param appData Application data containing current project rendering state.
 * @return Project segmentation display settings suitable for project JSON serialization.
 */
serialize::ProjectSegmentationDisplaySettings segmentationDisplaySettings(const AppData& appData);

/**
 * @brief Apply project-owned segmentation display settings to the application data.
 * @param appData Application data to update.
 * @param settings Serialized segmentation display settings to restore.
 */
void applySegmentationDisplaySettings(AppData& appData, const serialize::ProjectSegmentationDisplaySettings& settings);

/**
 * @brief Build project-owned 2D isocontour settings from the current application data.
 * @param appData Application data containing current project rendering state.
 * @return Project isocontour settings suitable for project JSON serialization.
 */
serialize::ProjectIsocontourDisplaySettings isocontourDisplaySettings(const AppData& appData);

/**
 * @brief Apply project-owned 2D isocontour settings to the application data.
 * @param appData Application data to update.
 * @param settings Serialized isocontour settings to restore.
 */
void applyIsocontourDisplaySettings(AppData& appData, const serialize::ProjectIsocontourDisplaySettings& settings);

/**
 * @brief Reset project-wide settings to built-in defaults.
 *
 * This resets only project-level display/review settings. It does not modify loaded images,
 * segmentations, landmarks, annotations, layouts, affine transforms, or deformation warp
 * assignments.
 *
 * @param appData Application data to update.
 */
void applyDefaultProjectSettings(AppData& appData);

/// @}

/**
 * @brief Synchronize layout-tab UI state after loading app or project settings.
 * @param appData Application data containing persistent settings and transient GUI state.
 */
void syncLayoutTabGuiData(AppData& appData);

/**
 * @brief Convert a runtime component render mode to its project serialization value.
 * @param mode Runtime component render mode.
 * @return Serialized component render mode.
 */
serialize::ProjectComponentRenderMode toSerializedComponentRenderMode(ComponentRenderMode mode);

/**
 * @brief Convert a serialized component render mode to its runtime value.
 * @param mode Serialized component render mode.
 * @return Runtime component render mode.
 */
ComponentRenderMode fromSerializedComponentRenderMode(serialize::ProjectComponentRenderMode mode);

/// Convert runtime phase units to their project representation; unknown values fall back to radians.
serialize::ProjectComplexPhaseUnit toSerializedComplexPhaseUnit(ComplexPhaseUnit unit);

/// Convert project phase units to runtime units; unknown values fall back to radians.
ComplexPhaseUnit fromSerializedComplexPhaseUnit(serialize::ProjectComplexPhaseUnit unit);

/// Convert a runtime phase range to its project representation; unknown values fall back to signed.
serialize::ProjectComplexPhaseRange toSerializedComplexPhaseRange(ComplexPhaseRange range);

/// Convert a project phase range to its runtime representation; unknown values fall back to signed.
ComplexPhaseRange fromSerializedComplexPhaseRange(serialize::ProjectComplexPhaseRange range);

/// Convert runtime arrow-spacing units to their project representation; unknown values fall back to voxels.
serialize::ProjectVectorArrowOverlaySpacingMode toSerializedVectorArrowOverlaySpacingMode(
  VectorArrowOverlaySpacingMode mode);

/// Convert project arrow-spacing units to runtime units; unknown values fall back to voxels.
VectorArrowOverlaySpacingMode fromSerializedVectorArrowOverlaySpacingMode(
  serialize::ProjectVectorArrowOverlaySpacingMode mode);

/// Convert the runtime grid convention to its project representation; unknown values fall back to SamplingField.
serialize::ProjectVectorWarpedGridConvention toSerializedVectorWarpedGridConvention(
  VectorWarpedGridConvention convention);

/// Convert the project grid convention to its runtime representation; unknown values fall back to SamplingField.
VectorWarpedGridConvention fromSerializedVectorWarpedGridConvention(
  serialize::ProjectVectorWarpedGridConvention convention);

/**
 * @brief Check whether a component rendering mode is valid for an image.
 * @param mode Requested component rendering mode.
 * @param image Image whose component count determines valid modes.
 * @return True when the image can render using the requested mode.
 */
bool componentRenderModeIsValidForImage(ComponentRenderMode mode, const Image& image);
} // namespace project_snapshot
