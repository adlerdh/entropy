#include "rendering/mesh/MeshIsosurfacePolicy.h"

namespace rendering::mesh
{

bool canRenderIsosurfaceWithMesh(const IsosurfaceMeshEligibility& eligibility) noexcept
{
  if (!eligibility.visible || eligibility.opacity <= 0.0f) {
    return false;
  }

  // Keep the handoff conservative. Raycasting remains the interactive path while the isovalue is actively changing,
  // and warping, transparency, and rim lighting still have raycast-only behavior.
  return !eligibility.renderWarped && !eligibility.rimLightingEnabled && !eligibility.valueEditInProgress &&
         eligibility.opacity >= 0.999f;
}

IsosurfaceMeshRequest makeScalarGridIsosurfaceRequest(
  const uuids::uuid& imageUid,
  const uint64_t imageDataVersion,
  const uint64_t imageGeometryVersion,
  const uint32_t component,
  const uint32_t timePoint,
  const double isoValue)
{
  return IsosurfaceMeshRequest{
    .imageUid = imageUid,
    .imageDataVersion = imageDataVersion,
    .imageGeometryVersion = imageGeometryVersion,
    .component = component,
    .timePoint = timePoint,
    .isoValue = isoValue,
    .algorithm = kScalarGridIsosurfaceAlgorithm,
    .algorithmVersion = kScalarGridIsosurfaceAlgorithmVersion};
}

} // namespace rendering::mesh
