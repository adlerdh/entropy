#pragma once

#include "rendering/mesh/MeshDrawOptions.h"
#include "rendering/mesh/MeshHandle.h"
#include "rendering/mesh/MeshMaterial.h"
#include "rendering/mesh/MeshRenderable.h"

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include <cstdint>

namespace rendering::mesh
{

/**
 * @brief Style resolved from one isosurface definition
 */
struct IsosurfaceMeshStyle
{
  MeshMaterial material;                                             //!< Surface material
  MeshCompositingMode compositingMode = MeshCompositingMode::Opaque; //!< Compositing path
  MeshFillMode fillMode = MeshFillMode::Surface;                     //!< Mesh fill mode
  bool backfaceCulling = false;                                      //!< Whether back-facing triangles may be culled
  bool visible = true;                                               //!< Whether the surface is visible
};

/**
 * @brief Style resolved from one segmentation label
 */
struct SegmentationLabelMeshStyle
{
  int64_t labelValue = 0;                                            //!< Segmentation label represented by the mesh
  MeshMaterial material;                                             //!< Surface material
  MeshCompositingMode compositingMode = MeshCompositingMode::Opaque; //!< Compositing path
  MeshFillMode fillMode = MeshFillMode::Surface;                     //!< Mesh fill mode
  bool backfaceCulling = false;                                      //!< Whether back-facing triangles may be culled
  bool visible = true;                                               //!< Whether the label is visible
};

/**
 * @brief Build a renderable for one extracted isosurface mesh
 * @param mesh Uploaded or uploadable mesh handle
 * @param world_T_mesh Transform from mesh coordinates to world coordinates
 * @param style Isosurface style resolved from application state
 * @return Mesh renderable
 */
MeshRenderable
makeIsosurfaceRenderable(MeshHandle mesh, const glm::mat4& world_T_mesh, const IsosurfaceMeshStyle& style);

/**
 * @brief Build a renderable for one extracted segmentation label mesh
 * @param mesh Uploaded or uploadable mesh handle
 * @param world_T_mesh Transform from mesh coordinates to world coordinates
 * @param style Label style resolved from a label table
 * @return Mesh renderable
 */
MeshRenderable makeSegmentationLabelRenderable(
  MeshHandle mesh,
  const glm::mat4& world_T_mesh,
  const SegmentationLabelMeshStyle& style);

} // namespace rendering::mesh
