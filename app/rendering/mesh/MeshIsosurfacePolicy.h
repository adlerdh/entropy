#pragma once

#include "rendering/mesh/MeshExtraction.h"

#include <cstdint>

namespace rendering::mesh
{

/**
 * @brief Algorithm name used by the first built-in scalar-grid isosurface extractor
 */
inline constexpr const char* kScalarGridIsosurfaceAlgorithm = "scalar-grid-marching-tetrahedra";

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
  bool rimLightingEnabled = false;  //!< True when raycast-only rim lighting is enabled
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
