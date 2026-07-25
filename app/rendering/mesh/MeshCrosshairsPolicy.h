#pragma once

#include "rendering/mesh/MeshGlyphs.h"

#include <glm/vec4.hpp>

namespace rendering::mesh
{

/**
 * @brief Inputs needed to decide whether and how to draw the crosshairs glyph in mesh-rendered 3D views
 */
struct MeshCrosshairsGlyphInputs
{
  bool showCrosshairsIn3D = true;       //!< User-facing 3D crosshairs glyph visibility setting
  bool cameraFollowsCrosshairs = false; //!< Whether the 3D camera eye follows the crosshairs position
  float diameterVoxelDiagonals = 2.0f;  //!< Glyph diameter in voxel-diagonal units
  float voxelDiagonalWorld = 1.0f;      //!< Voxel diagonal length in world units
  glm::vec4 color = glm::vec4{1.0f};    //!< Normalized non-premultiplied RGBA
};

/**
 * @brief Return whether the mesh crosshairs glyph should be visible
 * @param inputs Crosshairs glyph policy inputs
 * @return True when a nonzero glyph should be rendered
 */
bool shouldRenderMeshCrosshairsGlyph(const MeshCrosshairsGlyphInputs& inputs) noexcept;

/**
 * @brief Build the sphere-glyph style used for mesh-rendered crosshairs
 * @param inputs Crosshairs glyph policy inputs
 * @return Sphere glyph style with radius in world units
 */
MeshSphereGlyphStyle meshCrosshairsSphereGlyphStyle(const MeshCrosshairsGlyphInputs& inputs) noexcept;

} // namespace rendering::mesh
