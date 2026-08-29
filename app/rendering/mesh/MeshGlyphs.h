#pragma once

#include "rendering/mesh/MeshHandle.h"
#include "rendering/mesh/MeshRenderable.h"

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace rendering::mesh
{

/**
 * @brief World-space sphere glyph style
 */
struct MeshSphereGlyphStyle
{
  float radiusWorld = 1.0f;                                          //!< Sphere radius in world units
  glm::vec4 color = glm::vec4{1.0f};                                 //!< Normalized non-premultiplied RGBA
  MeshCompositingMode compositingMode = MeshCompositingMode::Opaque; //!< Compositing path
  bool visible = true;                                               //!< Whether the glyph is visible
};

/**
 * @brief World-space cylinder glyph style
 */
struct MeshCylinderGlyphStyle
{
  float radiusWorld = 1.0f;                                          //!< Cylinder radius in world units
  float lengthWorld = 1.0f;                                          //!< Cylinder length in world units
  glm::vec4 color = glm::vec4{1.0f};                                 //!< Normalized non-premultiplied RGBA
  MeshCompositingMode compositingMode = MeshCompositingMode::Opaque; //!< Compositing path
  bool visible = true;                                               //!< Whether the glyph is visible
};

/**
 * @brief Build a sphere glyph renderable from a canonical unit-radius sphere mesh
 * @param sphereMesh Canonical sphere mesh handle
 * @param centerWorld Glyph center in world coordinates
 * @param style World-space glyph style
 * @return Mesh renderable
 */
MeshRenderable makeSphereGlyphRenderable(
  const MeshHandle& sphereMesh,
  const glm::vec3& centerWorld,
  const MeshSphereGlyphStyle& style);

/**
 * @brief Build a z-axis cylinder glyph renderable from a canonical unit-radius, unit-length cylinder mesh
 * @param cylinderMesh Canonical cylinder mesh handle
 * @param centerWorld Glyph center in world coordinates
 * @param style World-space glyph style
 * @return Mesh renderable
 */
MeshRenderable makeZAxisCylinderGlyphRenderable(
  const MeshHandle& cylinderMesh,
  const glm::vec3& centerWorld,
  const MeshCylinderGlyphStyle& style);

} // namespace rendering::mesh
