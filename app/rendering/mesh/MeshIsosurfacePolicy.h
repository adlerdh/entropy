#pragma once

#include "rendering/mesh/MeshExtraction.h"
#include "rendering/mesh/MeshRenderableFactory.h"

#include <cstdint>

namespace rendering::mesh
{

/**
 * @brief Algorithm name used by the built-in scalar-grid isosurface extractor
 */
inline constexpr const char* kScalarGridIsosurfaceAlgorithm = "scalar-grid-vtk-flying-edges-3d";

/**
 * @brief Version for the built-in scalar-grid isosurface extractor
 */
inline constexpr uint64_t kScalarGridIsosurfaceAlgorithmVersion = 1;

/**
 * @brief Inputs that determine whether an isosurface can use the current mesh renderer without changing appearance
 */
struct IsosurfaceMeshEligibility
{
  bool renderWarped = false;        //!< True when the raycast path applies an inverse warp field
  bool valueEditInProgress = false; //!< True while the user is interactively changing the isovalue
  float opacity = 1.0f;             //!< Effective opacity after image-level modulation
  bool visible = true;              //!< User-visible surface flag
};

/**
 * @brief Return whether the current mesh path can replace raycasting for one isosurface
 * @param eligibility Isosurface state relevant to conservative mesh replacement
 * @return True when the mesh path can represent the surface without a known visual regression
 */
bool canRenderIsosurfaceWithMesh(const IsosurfaceMeshEligibility& eligibility) noexcept;

/**
 * @brief Return whether raycasting can be replaced by the exact current mesh.
 *
 * Eligibility alone is insufficient: the extracted geometry must also have reached the GPU before the handoff.
 */
bool isosurfaceMeshReadyForHandoff(const IsosurfaceMeshEligibility& eligibility, bool gpuMeshReady) noexcept;

/** Return whether a render mode must use the live raycast preview for an actively edited isosurface. */
bool useRaycastPreviewDuringIsosurfaceEdit(bool renderModeIncludesIsosurfaces, bool activeEdit) noexcept;

/**
 * @brief Select the mesh compositing path for an isosurface appearance
 * @param alpha Effective non-premultiplied surface alpha
 * @param rimLightingEnabled Whether view-angle rim lighting is active
 * @param rimOpacityStrength Strength of view-angle opacity modulation
 * @param translucentMode Compositing path used for translucent surfaces
 * @return Opaque compositing for fully opaque surfaces, otherwise the requested translucent compositing path
 */
MeshCompositingMode compositingModeForIsosurfaceAlpha(
  float alpha,
  bool rimLightingEnabled = false,
  float rimOpacityStrength = 0.0f,
  MeshCompositingMode translucentMode = MeshCompositingMode::AlphaOverDdp) noexcept;

/**
 * @brief Build an extraction request for one image isosurface using the built-in scalar-grid backend
 * @param imageUid Source image
 * @param imageDataVersion Version of the source image voxel values
 * @param imageGeometryVersion Version of source image geometry and transforms
 * @param component Scalar image component
 * @param timePoint Time frame
 * @param isoValue Isovalue
 * @return Cacheable extraction request
 */
IsosurfaceMeshRequest makeScalarGridIsosurfaceRequest(
  const uuids::uuid& imageUid,
  uint64_t imageDataVersion,
  uint64_t imageGeometryVersion,
  uint32_t component,
  uint32_t timePoint,
  double isoValue);

} // namespace rendering::mesh
