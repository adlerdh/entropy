#pragma once

#include "image/ImageSettings.h"
#include "logic/serialization/ProjectSerialization.h"

#include <algorithm>
#include <cstdint>
#include <string>

class AppData;
class Image;

namespace project_snapshot
{
/**
 * @brief Build serialized image settings from the runtime image state.
 * @param image Image whose display settings should be serialized.
 * @return Project image settings suitable for project JSON serialization.
 */
serialize::ImageSettings imageSettings(const Image& image);

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

/**
 * @brief Build serialized interface settings from the current application data.
 * @param appData Application data containing the current interface state.
 * @return Project interface settings suitable for project JSON serialization.
 */
serialize::ProjectInterfaceSettings interfaceSettings(const AppData& appData);

/**
 * @brief Apply serialized interface settings to the application data.
 * @param appData Application data to update.
 * @param settings Serialized interface settings to restore.
 */
void applyInterfaceSettings(AppData& appData, const serialize::ProjectInterfaceSettings& settings);

/**
 * @brief Synchronize layout-tab UI state after loading app or project settings.
 * @param appData Application data containing persistent settings and transient GUI state.
 */
void syncLayoutTabGuiData(AppData& appData);

/**
 * @brief Clamp saved precision values to the supported display range.
 * @param precision Requested number of fractional digits.
 * @return Supported precision value.
 */
inline std::uint32_t clampPrecision(std::uint32_t precision)
{
  constexpr std::uint32_t kMaxPrecision = 9;
  return std::min(precision, kMaxPrecision);
}

/**
 * @brief Create a printf-style floating-point format for a saved precision value.
 * @param precision Requested number of fractional digits.
 * @return Format string such as "%0.3f".
 */
inline std::string precisionFormat(std::uint32_t precision)
{
  return std::string{"%0."} + std::to_string(clampPrecision(precision)) + "f";
}

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

  return serialize::ProjectVectorArrowOverlaySpacingMode::Pixels;
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

  return VectorArrowOverlaySpacingMode::Pixels;
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
