#include "logic/app/ProjectSnapshotSettings.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Project snapshot precision values are clamped and formatted", "[ProjectSnapshotSettings]")
{
  CHECK(project_snapshot::clampPrecision(0) == 0);
  CHECK(project_snapshot::clampPrecision(4) == 4);
  CHECK(project_snapshot::clampPrecision(100) == 9);

  CHECK(project_snapshot::precisionFormat(0) == "%0.0f");
  CHECK(project_snapshot::precisionFormat(3) == "%0.3f");
  CHECK(project_snapshot::precisionFormat(100) == "%0.9f");
}

TEST_CASE("Project snapshot component render modes round trip through serialization", "[ProjectSnapshotSettings]")
{
  CHECK(
    project_snapshot::fromSerializedComponentRenderMode(project_snapshot::toSerializedComponentRenderMode(
      ComponentRenderMode::SingleComponent)) == ComponentRenderMode::SingleComponent);
  CHECK(
    project_snapshot::fromSerializedComponentRenderMode(
      project_snapshot::toSerializedComponentRenderMode(ComponentRenderMode::Color)) == ComponentRenderMode::Color);
  CHECK(
    project_snapshot::fromSerializedComponentRenderMode(
      project_snapshot::toSerializedComponentRenderMode(ComponentRenderMode::Minimum)) == ComponentRenderMode::Minimum);
  CHECK(
    project_snapshot::fromSerializedComponentRenderMode(
      project_snapshot::toSerializedComponentRenderMode(ComponentRenderMode::Mean)) == ComponentRenderMode::Mean);
  CHECK(
    project_snapshot::fromSerializedComponentRenderMode(
      project_snapshot::toSerializedComponentRenderMode(ComponentRenderMode::Maximum)) == ComponentRenderMode::Maximum);
  CHECK(
    project_snapshot::fromSerializedComponentRenderMode(project_snapshot::toSerializedComponentRenderMode(
      ComponentRenderMode::Magnitude)) == ComponentRenderMode::Magnitude);
  CHECK(
    project_snapshot::fromSerializedComponentRenderMode(project_snapshot::toSerializedComponentRenderMode(
      ComponentRenderMode::ComplexPhase)) == ComponentRenderMode::ComplexPhase);
  CHECK(
    project_snapshot::fromSerializedComponentRenderMode(project_snapshot::toSerializedComponentRenderMode(
      ComponentRenderMode::ComplexReal)) == ComponentRenderMode::ComplexReal);
  CHECK(
    project_snapshot::fromSerializedComponentRenderMode(project_snapshot::toSerializedComponentRenderMode(
      ComponentRenderMode::ComplexImaginary)) == ComponentRenderMode::ComplexImaginary);
  CHECK(
    project_snapshot::fromSerializedComponentRenderMode(project_snapshot::toSerializedComponentRenderMode(
      ComponentRenderMode::VectorDirectionColor)) == ComponentRenderMode::VectorDirectionColor);
  CHECK(
    project_snapshot::fromSerializedComponentRenderMode(project_snapshot::toSerializedComponentRenderMode(
      ComponentRenderMode::VectorSignedNormalProjection)) == ComponentRenderMode::VectorSignedNormalProjection);
  CHECK(
    project_snapshot::fromSerializedComponentRenderMode(project_snapshot::toSerializedComponentRenderMode(
      ComponentRenderMode::VectorPlanarProjectionColor)) == ComponentRenderMode::VectorPlanarProjectionColor);
  CHECK(
    project_snapshot::fromSerializedComponentRenderMode(project_snapshot::toSerializedComponentRenderMode(
      ComponentRenderMode::VectorJacobianDeterminant)) == ComponentRenderMode::VectorJacobianDeterminant);
  CHECK(
    project_snapshot::fromSerializedComponentRenderMode(project_snapshot::toSerializedComponentRenderMode(
      ComponentRenderMode::VectorGradientMagnitude)) == ComponentRenderMode::VectorGradientMagnitude);
  CHECK(
    project_snapshot::fromSerializedComponentRenderMode(project_snapshot::toSerializedComponentRenderMode(
      ComponentRenderMode::VectorDivergence)) == ComponentRenderMode::VectorDivergence);
  CHECK(
    project_snapshot::fromSerializedComponentRenderMode(project_snapshot::toSerializedComponentRenderMode(
      ComponentRenderMode::VectorCurlMagnitude)) == ComponentRenderMode::VectorCurlMagnitude);
  CHECK(
    project_snapshot::fromSerializedComponentRenderMode(project_snapshot::toSerializedComponentRenderMode(
      ComponentRenderMode::VectorLaplacianMagnitude)) == ComponentRenderMode::VectorLaplacianMagnitude);
  CHECK(
    project_snapshot::fromSerializedComplexPhaseUnit(
      project_snapshot::toSerializedComplexPhaseUnit(ComplexPhaseUnit::Degrees)) == ComplexPhaseUnit::Degrees);
  CHECK(
    project_snapshot::fromSerializedComplexPhaseRange(
      project_snapshot::toSerializedComplexPhaseRange(ComplexPhaseRange::Unsigned)) == ComplexPhaseRange::Unsigned);
  CHECK(
    project_snapshot::fromSerializedVectorArrowOverlaySpacingMode(
      project_snapshot::toSerializedVectorArrowOverlaySpacingMode(VectorArrowOverlaySpacingMode::Millimeters)) ==
    VectorArrowOverlaySpacingMode::Millimeters);
  CHECK(
    project_snapshot::fromSerializedVectorWarpedGridConvention(project_snapshot::toSerializedVectorWarpedGridConvention(
      VectorWarpedGridConvention::ApparentDeformation)) == VectorWarpedGridConvention::ApparentDeformation);
}
