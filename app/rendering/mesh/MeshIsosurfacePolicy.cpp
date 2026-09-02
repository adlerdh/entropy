#include "rendering/mesh/MeshIsosurfacePolicy.h"

#include <bit>

namespace rendering::mesh
{

bool canRenderIsosurfaceWithMesh(const IsosurfaceMeshEligibility& eligibility) noexcept
{
  if (!eligibility.visible || eligibility.opacity <= 0.0f) {
    return false;
  }

  // Keep the handoff conservative. Raycasting remains the interactive path while the isovalue is actively changing,
  // and warping still has raycast-only behavior because mesh extraction currently uses the unwarped image grid.
  return !eligibility.renderWarped && !eligibility.valueEditInProgress;
}

bool isosurfaceMeshReadyForHandoff(const IsosurfaceMeshEligibility& eligibility, const bool gpuMeshReady) noexcept
{
  return canRenderIsosurfaceWithMesh(eligibility) && gpuMeshReady;
}

bool useRaycastPreviewDuringIsosurfaceEdit(const bool renderModeIncludesIsosurfaces, const bool activeEdit) noexcept
{
  return renderModeIncludesIsosurfaces && activeEdit;
}

MeshCompositingMode compositingModeForIsosurfaceAlpha(
  const float alpha,
  const bool rimLightingEnabled,
  const float rimOpacityStrength,
  const MeshCompositingMode translucentMode) noexcept
{
  const bool rimOpacityCanVary = rimLightingEnabled && rimOpacityStrength > 0.0f;
  return alpha >= 0.999f && !rimOpacityCanVary ? MeshCompositingMode::Opaque : translucentMode;
}

IsosurfaceMeshRequest makeScalarGridIsosurfaceRequest(
  const uuids::uuid& imageUid,
  const uint64_t imageDataVersion,
  const uint64_t imageGeometryVersion,
  const uint32_t component,
  const uint32_t timePoint,
  const double isoValue,
  const MeshGenerationOptions& generationOptions)
{
  // Thread count affects execution only. Smoothing values change geometry and therefore must invalidate the cache.
  uint64_t algorithmVersion = kScalarGridIsosurfaceAlgorithmVersion;
  algorithmVersion ^= static_cast<uint64_t>(generationOptions.smoothSurface) + 0x9e3779b97f4a7c15ULL +
                      (algorithmVersion << 6U) + (algorithmVersion >> 2U);
  algorithmVersion ^= static_cast<uint64_t>(generationOptions.smoothingIterations) + 0x9e3779b97f4a7c15ULL +
                      (algorithmVersion << 6U) + (algorithmVersion >> 2U);
  algorithmVersion ^= std::bit_cast<uint64_t>(generationOptions.smoothingPassBand) + 0x9e3779b97f4a7c15ULL +
                      (algorithmVersion << 6U) + (algorithmVersion >> 2U);
  return IsosurfaceMeshRequest{
    .imageUid = imageUid,
    .imageDataVersion = imageDataVersion,
    .imageGeometryVersion = imageGeometryVersion,
    .component = component,
    .timePoint = timePoint,
    .isoValue = isoValue,
    .algorithm = kScalarGridIsosurfaceAlgorithm,
    .algorithmVersion = algorithmVersion};
}

} // namespace rendering::mesh
