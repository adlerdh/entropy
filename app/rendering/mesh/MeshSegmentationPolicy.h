#pragma once

#include "rendering/mesh/MeshExtraction.h"
#include "rendering/mesh/MeshGeneration.h"
#include "rendering/mesh/MeshRenderableFactory.h"

#include <glm/vec4.hpp>

#include <cstdint>

namespace rendering::mesh
{

/**
 * @brief Algorithm name used by the built-in scalar-grid segmentation-label extractor
 */
inline constexpr const char* kScalarGridSegmentationAlgorithm = "scalar-grid-vtk-discrete-flying-edges-3d";

/**
 * @brief Version for the built-in scalar-grid segmentation-label extractor
 */
inline constexpr uint64_t kScalarGridSegmentationAlgorithmVersion = 2;

/**
 * @brief User-facing label-table state needed to decide if a segmentation label should render as a mesh
 */
struct SegmentationLabelMeshState
{
  bool showMesh = true; //!< 3D mesh visibility
  float opacity = 1.0f; //!< Effective label opacity after segmentation-level modulation
};

/**
 * @brief Return whether a segmentation label should produce and render a mesh
 * @param state Label mesh visibility and segmentation opacity state
 * @return True when a non-transparent label mesh is enabled in 3D
 */
bool shouldRenderSegmentationLabelMesh(const SegmentationLabelMeshState& state) noexcept;

/**
 * @brief Select the mesh compositing path for a label alpha value
 * @param alpha Effective non-premultiplied label alpha
 * @param translucentMode Compositing path used for translucent labels
 * @return Opaque compositing for fully opaque labels, otherwise the requested translucent compositing path
 */
MeshCompositingMode compositingModeForLabelAlpha(
  float alpha,
  MeshCompositingMode translucentMode = MeshCompositingMode::AlphaOverDdp) noexcept;

/**
 * @brief Convert normalized RGBA and visibility state into a renderable label style
 * @param labelValue Segmentation label value
 * @param color Normalized non-premultiplied RGBA
 * @param state Label visibility and opacity state
 * @param translucentMode Compositing path used for translucent labels
 * @return Renderable style for one label
 */
SegmentationLabelMeshStyle segmentationLabelMeshStyle(
  int64_t labelValue,
  const glm::vec4& color,
  const SegmentationLabelMeshState& state,
  MeshCompositingMode translucentMode = MeshCompositingMode::AlphaOverDdp) noexcept;

/**
 * @brief Build an extraction request for one segmentation label using the built-in scalar-grid backend
 * @param segmentationUid Source segmentation image
 * @param segmentationDataVersion Version of the source segmentation label values
 * @param segmentationGeometryVersion Version of source segmentation geometry and transforms
 * @param labelValue Label value, equal to the label table index used by Entropy segmentations
 * @param timePoint Time frame
 * @return Cacheable extraction request
 */
SegmentationMeshRequest makeScalarGridSegmentationRequest(
  const uuids::uuid& segmentationUid,
  uint64_t segmentationDataVersion,
  uint64_t segmentationGeometryVersion,
  int64_t labelValue,
  uint32_t timePoint,
  const MeshGenerationOptions& generationOptions = {});

} // namespace rendering::mesh
