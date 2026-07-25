#include "rendering/mesh/MeshSegmentationPolicy.h"

namespace rendering::mesh
{

bool shouldRenderSegmentationLabelMesh(const SegmentationLabelMeshState& state) noexcept
{
  return state.visible && state.showMesh && state.opacity > 0.0f;
}

MeshCompositingMode compositingModeForLabelAlpha(const float alpha, const MeshCompositingMode translucentMode) noexcept
{
  return alpha >= 0.999f ? MeshCompositingMode::Opaque : translucentMode;
}

SegmentationLabelMeshStyle segmentationLabelMeshStyle(
  const int64_t labelValue,
  const glm::vec4& color,
  const SegmentationLabelMeshState& state,
  const MeshCompositingMode translucentMode) noexcept
{
  const float alpha = color.a * state.opacity;
  return SegmentationLabelMeshStyle{
    .labelValue = labelValue,
    .material = {.baseColor = glm::vec4{color.r, color.g, color.b, alpha}},
    .compositingMode = compositingModeForLabelAlpha(alpha, translucentMode),
    .visible = shouldRenderSegmentationLabelMesh(state)};
}

SegmentationMeshRequest makeScalarGridSegmentationRequest(
  const uuids::uuid& segmentationUid,
  const uint64_t segmentationDataVersion,
  const uint64_t segmentationGeometryVersion,
  const int64_t labelValue,
  const uint32_t timePoint)
{
  return SegmentationMeshRequest{
    .segmentationUid = segmentationUid,
    .segmentationDataVersion = segmentationDataVersion,
    .segmentationGeometryVersion = segmentationGeometryVersion,
    .labelValue = labelValue,
    .timePoint = timePoint,
    .algorithm = kScalarGridSegmentationAlgorithm,
    .algorithmVersion = kScalarGridSegmentationAlgorithmVersion};
}

} // namespace rendering::mesh
