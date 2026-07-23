#pragma once

#include "image/ImageSettings.h"
#include "logic/serialization/ProjectSerialization.h"

#include <optional>

class AppData;
class Image;

namespace project_snapshot
{
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
 * @param segmentation Segmentation image whose settings should be serialized.
 * @return Project segmentation settings suitable for project JSON serialization.
 */
serialize::SegSettings segmentationSettings(const Image& segmentation);

/**
 * @brief Apply serialized segmentation settings to a loaded segmentation.
 * @param segmentation Segmentation image to update.
 * @param settings Serialized settings to restore.
 */
void applySegmentationSettings(Image& segmentation, const serialize::SegSettings& settings);

/// @}

/**
 * @name Project-wide settings
 *
 * These helpers are the ownership boundary for values stored live in `AppData`/`RenderData` but
 * persisted with the project. Application preferences must not write these values.
 */
/// @{

/**
 * @brief Build project-owned interface settings from the current application data.
 * @param appData Application data containing the current project review state.
 * @return Project interface settings suitable for project JSON serialization.
 */
serialize::ProjectInterfaceSettings interfaceSettings(const AppData& appData);

/**
 * @brief Apply project-owned interface settings to the application data.
 * @param appData Application data to update.
 * @param settings Serialized interface settings to restore.
 */
void applyInterfaceSettings(AppData& appData, const serialize::ProjectInterfaceSettings& settings);

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
 * @brief Build project-owned isosurface display settings from the current application data.
 * @param appData Application data containing current project rendering state.
 * @return Project isosurface display settings suitable for project JSON serialization.
 */
serialize::ProjectIsosurfaceDisplaySettings isosurfaceDisplaySettings(const AppData& appData);

/**
 * @brief Apply project-owned isosurface display settings to the application data.
 * @param appData Application data to update.
 * @param settings Serialized isosurface display settings to restore.
 */
void applyIsosurfaceDisplaySettings(AppData& appData, const serialize::ProjectIsosurfaceDisplaySettings& settings);

/**
 * @brief Build project-owned annotation display settings from the current application data.
 * @param appData Application data containing current project annotation and landmark rendering state.
 * @return Project annotation display settings suitable for project JSON serialization.
 */
serialize::ProjectAnnotationDisplaySettings annotationDisplaySettings(const AppData& appData);

/**
 * @brief Apply project-owned annotation display settings to the application data.
 * @param appData Application data to update.
 * @param settings Serialized annotation display settings to restore.
 */
void applyAnnotationDisplaySettings(AppData& appData, const serialize::ProjectAnnotationDisplaySettings& settings);

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
inline serialize::ProjectComponentRenderMode toSerializedComponentRenderMode(ComponentRenderMode mode)
{
  switch (mode) {
    case ComponentRenderMode::SingleComponent:
      return serialize::ProjectComponentRenderMode::SingleComponent;
    case ComponentRenderMode::Color:
      return serialize::ProjectComponentRenderMode::Color;
    case ComponentRenderMode::Minimum:
      return serialize::ProjectComponentRenderMode::Minimum;
    case ComponentRenderMode::Mean:
      return serialize::ProjectComponentRenderMode::Mean;
    case ComponentRenderMode::Maximum:
      return serialize::ProjectComponentRenderMode::Maximum;
    case ComponentRenderMode::Magnitude:
      return serialize::ProjectComponentRenderMode::Magnitude;
    case ComponentRenderMode::ComplexPhase:
      return serialize::ProjectComponentRenderMode::ComplexPhase;
    case ComponentRenderMode::ComplexReal:
      return serialize::ProjectComponentRenderMode::ComplexReal;
    case ComponentRenderMode::ComplexImaginary:
      return serialize::ProjectComponentRenderMode::ComplexImaginary;
    case ComponentRenderMode::VectorDirectionColor:
      return serialize::ProjectComponentRenderMode::VectorDirectionColor;
    case ComponentRenderMode::VectorSignedNormalProjection:
      return serialize::ProjectComponentRenderMode::VectorSignedNormalProjection;
    case ComponentRenderMode::VectorPlanarProjectionColor:
      return serialize::ProjectComponentRenderMode::VectorPlanarProjectionColor;
    case ComponentRenderMode::VectorJacobianDeterminant:
      return serialize::ProjectComponentRenderMode::VectorJacobianDeterminant;
    case ComponentRenderMode::VectorGradientMagnitude:
      return serialize::ProjectComponentRenderMode::VectorGradientMagnitude;
    case ComponentRenderMode::VectorDivergence:
      return serialize::ProjectComponentRenderMode::VectorDivergence;
    case ComponentRenderMode::VectorCurlMagnitude:
      return serialize::ProjectComponentRenderMode::VectorCurlMagnitude;
    case ComponentRenderMode::VectorLaplacianMagnitude:
      return serialize::ProjectComponentRenderMode::VectorLaplacianMagnitude;
  }

  return serialize::ProjectComponentRenderMode::SingleComponent;
}

/**
 * @brief Convert a serialized component render mode to its runtime value.
 * @param mode Serialized component render mode.
 * @return Runtime component render mode.
 */
inline ComponentRenderMode fromSerializedComponentRenderMode(serialize::ProjectComponentRenderMode mode)
{
  switch (mode) {
    case serialize::ProjectComponentRenderMode::SingleComponent:
      return ComponentRenderMode::SingleComponent;
    case serialize::ProjectComponentRenderMode::Color:
      return ComponentRenderMode::Color;
    case serialize::ProjectComponentRenderMode::Minimum:
      return ComponentRenderMode::Minimum;
    case serialize::ProjectComponentRenderMode::Mean:
      return ComponentRenderMode::Mean;
    case serialize::ProjectComponentRenderMode::Maximum:
      return ComponentRenderMode::Maximum;
    case serialize::ProjectComponentRenderMode::Magnitude:
      return ComponentRenderMode::Magnitude;
    case serialize::ProjectComponentRenderMode::ComplexPhase:
      return ComponentRenderMode::ComplexPhase;
    case serialize::ProjectComponentRenderMode::ComplexReal:
      return ComponentRenderMode::ComplexReal;
    case serialize::ProjectComponentRenderMode::ComplexImaginary:
      return ComponentRenderMode::ComplexImaginary;
    case serialize::ProjectComponentRenderMode::VectorDirectionColor:
      return ComponentRenderMode::VectorDirectionColor;
    case serialize::ProjectComponentRenderMode::VectorSignedNormalProjection:
      return ComponentRenderMode::VectorSignedNormalProjection;
    case serialize::ProjectComponentRenderMode::VectorPlanarProjectionColor:
      return ComponentRenderMode::VectorPlanarProjectionColor;
    case serialize::ProjectComponentRenderMode::VectorJacobianDeterminant:
      return ComponentRenderMode::VectorJacobianDeterminant;
    case serialize::ProjectComponentRenderMode::VectorGradientMagnitude:
      return ComponentRenderMode::VectorGradientMagnitude;
    case serialize::ProjectComponentRenderMode::VectorDivergence:
      return ComponentRenderMode::VectorDivergence;
    case serialize::ProjectComponentRenderMode::VectorCurlMagnitude:
      return ComponentRenderMode::VectorCurlMagnitude;
    case serialize::ProjectComponentRenderMode::VectorLaplacianMagnitude:
      return ComponentRenderMode::VectorLaplacianMagnitude;
  }

  return ComponentRenderMode::SingleComponent;
}

inline serialize::ProjectComplexPhaseUnit toSerializedComplexPhaseUnit(ComplexPhaseUnit unit)
{
  switch (unit) {
    case ComplexPhaseUnit::Radians:
      return serialize::ProjectComplexPhaseUnit::Radians;
    case ComplexPhaseUnit::Degrees:
      return serialize::ProjectComplexPhaseUnit::Degrees;
  }

  return serialize::ProjectComplexPhaseUnit::Radians;
}

inline ComplexPhaseUnit fromSerializedComplexPhaseUnit(serialize::ProjectComplexPhaseUnit unit)
{
  switch (unit) {
    case serialize::ProjectComplexPhaseUnit::Radians:
      return ComplexPhaseUnit::Radians;
    case serialize::ProjectComplexPhaseUnit::Degrees:
      return ComplexPhaseUnit::Degrees;
  }

  return ComplexPhaseUnit::Radians;
}

inline serialize::ProjectComplexPhaseRange toSerializedComplexPhaseRange(ComplexPhaseRange range)
{
  switch (range) {
    case ComplexPhaseRange::Signed:
      return serialize::ProjectComplexPhaseRange::Signed;
    case ComplexPhaseRange::Unsigned:
      return serialize::ProjectComplexPhaseRange::Unsigned;
  }

  return serialize::ProjectComplexPhaseRange::Signed;
}

inline ComplexPhaseRange fromSerializedComplexPhaseRange(serialize::ProjectComplexPhaseRange range)
{
  switch (range) {
    case serialize::ProjectComplexPhaseRange::Signed:
      return ComplexPhaseRange::Signed;
    case serialize::ProjectComplexPhaseRange::Unsigned:
      return ComplexPhaseRange::Unsigned;
  }

  return ComplexPhaseRange::Signed;
}

inline serialize::ProjectVectorArrowOverlaySpacingMode toSerializedVectorArrowOverlaySpacingMode(
  VectorArrowOverlaySpacingMode mode)
{
  switch (mode) {
    case VectorArrowOverlaySpacingMode::Pixels:
      return serialize::ProjectVectorArrowOverlaySpacingMode::Pixels;
    case VectorArrowOverlaySpacingMode::Voxels:
      return serialize::ProjectVectorArrowOverlaySpacingMode::Voxels;
    case VectorArrowOverlaySpacingMode::Millimeters:
      return serialize::ProjectVectorArrowOverlaySpacingMode::Millimeters;
  }

  return serialize::ProjectVectorArrowOverlaySpacingMode::Voxels;
}

inline VectorArrowOverlaySpacingMode fromSerializedVectorArrowOverlaySpacingMode(
  serialize::ProjectVectorArrowOverlaySpacingMode mode)
{
  switch (mode) {
    case serialize::ProjectVectorArrowOverlaySpacingMode::Pixels:
      return VectorArrowOverlaySpacingMode::Pixels;
    case serialize::ProjectVectorArrowOverlaySpacingMode::Voxels:
      return VectorArrowOverlaySpacingMode::Voxels;
    case serialize::ProjectVectorArrowOverlaySpacingMode::Millimeters:
      return VectorArrowOverlaySpacingMode::Millimeters;
  }

  return VectorArrowOverlaySpacingMode::Voxels;
}

inline serialize::ProjectVectorWarpedGridConvention toSerializedVectorWarpedGridConvention(
  VectorWarpedGridConvention convention)
{
  switch (convention) {
    case VectorWarpedGridConvention::SamplingField:
      return serialize::ProjectVectorWarpedGridConvention::SamplingField;
    case VectorWarpedGridConvention::ApparentDeformation:
      return serialize::ProjectVectorWarpedGridConvention::ApparentDeformation;
  }

  return serialize::ProjectVectorWarpedGridConvention::SamplingField;
}

inline VectorWarpedGridConvention fromSerializedVectorWarpedGridConvention(
  serialize::ProjectVectorWarpedGridConvention convention)
{
  switch (convention) {
    case serialize::ProjectVectorWarpedGridConvention::SamplingField:
      return VectorWarpedGridConvention::SamplingField;
    case serialize::ProjectVectorWarpedGridConvention::ApparentDeformation:
      return VectorWarpedGridConvention::ApparentDeformation;
  }

  return VectorWarpedGridConvention::SamplingField;
}

/**
 * @brief Check whether a component rendering mode is valid for an image.
 * @param mode Requested component rendering mode.
 * @param image Image whose component count determines valid modes.
 * @return True when the image can render using the requested mode.
 */
bool componentRenderModeIsValidForImage(ComponentRenderMode mode, const Image& image);
} // namespace project_snapshot
