#include "logic/app/ProjectSnapshotSettings.h"
#include "image/ImageSettings.h"
#include "logic/serialization/ProjectSerialization.h"

namespace project_snapshot
{
serialize::ProjectComponentRenderMode toSerializedComponentRenderMode(ComponentRenderMode mode)
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

ComponentRenderMode fromSerializedComponentRenderMode(serialize::ProjectComponentRenderMode mode)
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

serialize::ProjectComplexPhaseUnit toSerializedComplexPhaseUnit(ComplexPhaseUnit unit)
{
  switch (unit) {
    case ComplexPhaseUnit::Radians:
      return serialize::ProjectComplexPhaseUnit::Radians;
    case ComplexPhaseUnit::Degrees:
      return serialize::ProjectComplexPhaseUnit::Degrees;
  }

  return serialize::ProjectComplexPhaseUnit::Radians;
}

ComplexPhaseUnit fromSerializedComplexPhaseUnit(serialize::ProjectComplexPhaseUnit unit)
{
  switch (unit) {
    case serialize::ProjectComplexPhaseUnit::Radians:
      return ComplexPhaseUnit::Radians;
    case serialize::ProjectComplexPhaseUnit::Degrees:
      return ComplexPhaseUnit::Degrees;
  }

  return ComplexPhaseUnit::Radians;
}

serialize::ProjectComplexPhaseRange toSerializedComplexPhaseRange(ComplexPhaseRange range)
{
  switch (range) {
    case ComplexPhaseRange::Signed:
      return serialize::ProjectComplexPhaseRange::Signed;
    case ComplexPhaseRange::Unsigned:
      return serialize::ProjectComplexPhaseRange::Unsigned;
  }

  return serialize::ProjectComplexPhaseRange::Signed;
}

ComplexPhaseRange fromSerializedComplexPhaseRange(serialize::ProjectComplexPhaseRange range)
{
  switch (range) {
    case serialize::ProjectComplexPhaseRange::Signed:
      return ComplexPhaseRange::Signed;
    case serialize::ProjectComplexPhaseRange::Unsigned:
      return ComplexPhaseRange::Unsigned;
  }

  return ComplexPhaseRange::Signed;
}

serialize::ProjectVectorArrowOverlaySpacingMode toSerializedVectorArrowOverlaySpacingMode(
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

VectorArrowOverlaySpacingMode fromSerializedVectorArrowOverlaySpacingMode(
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

serialize::ProjectVectorWarpedGridConvention toSerializedVectorWarpedGridConvention(
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

VectorWarpedGridConvention fromSerializedVectorWarpedGridConvention(
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
} // namespace project_snapshot
