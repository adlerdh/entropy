#include "rendering/mesh/MeshSegmentationPolicy.h"

#include <glm/common.hpp>

namespace rendering::mesh
{

bool shouldRenderSegmentationLabelMesh(const SegmentationLabelMeshState& state) noexcept
{
  return state.showMesh && state.opacity > 0.0f;
}

float segmentationMeshOpacity(
  const float segmentationOpacity,
  const float imageOpacity,
  const bool modulateWithImageOpacity) noexcept
{
  const float opacity = modulateWithImageOpacity ? segmentationOpacity * imageOpacity : segmentationOpacity;
  return glm::clamp(opacity, 0.0f, 1.0f);
}

MeshCompositingMode compositingModeForLabelAlpha(const float alpha, const MeshCompositingMode translucentMode) noexcept
{
  return alpha >= 0.999f ? MeshCompositingMode::Opaque : translucentMode;
}

SegmentationLabelMeshStyle segmentationLabelMeshStyle(
  const int64_t labelValue,
  const glm::vec4& color,
  const SegmentationLabelMeshState& state,
  const MeshSurfaceMaterialSettings& materialSettings,
  const MeshCompositingMode translucentMode) noexcept
{
  const float alpha = color.a * state.opacity;
  return SegmentationLabelMeshStyle{
    .labelValue = labelValue,
    .material = meshMaterialForSurface(glm::vec4{color.r, color.g, color.b, alpha}, materialSettings),
    .compositingMode = compositingModeForLabelAlpha(alpha, translucentMode),
    // Only labels that actually touch another nonzero label need one-sided rasterization to avoid depth-testing two
    // coincident, oppositely wound boundaries. Isolated labels retain two-sided rendering and clipping behavior.
    .backfaceCulling = state.hasSharedBoundary,
    .visible = shouldRenderSegmentationLabelMesh(state)};
}

SegmentationMeshRequest makeScalarGridSegmentationRequest(
  const uuids::uuid& segmentationUid,
  const uint64_t segmentationDataVersion,
  const uint64_t segmentationGeometryVersion,
  const int64_t labelValue,
  const uint32_t timePoint,
  const MeshGenerationOptions& generationOptions)
{
  // Thread count affects execution only. Smoothing values change geometry and therefore must invalidate the cache.
  const uint64_t algorithmVersion =
    meshGenerationAlgorithmVersion(kScalarGridSegmentationAlgorithmVersion, generationOptions);
  return SegmentationMeshRequest{
    .segmentationUid = segmentationUid,
    .segmentationDataVersion = segmentationDataVersion,
    .segmentationGeometryVersion = segmentationGeometryVersion,
    .labelValue = labelValue,
    .timePoint = timePoint,
    .generationOptions = generationOptions,
    .algorithm = kScalarGridSegmentationAlgorithm,
    .algorithmVersion = algorithmVersion};
}

} // namespace rendering::mesh
