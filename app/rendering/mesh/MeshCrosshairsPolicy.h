#pragma once

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
  float lengthVoxelDiagonals = 16.0f;   //!< Per-axis glyph length in voxel-diagonal units
  float voxelDiagonalWorld = 1.0f;      //!< Voxel diagonal length in world units
};

struct MeshCrosshairsGlyphStyle
{
  float radiusWorld = 0.0f;
  float halfLengthWorld = 0.0f;
  bool visible = false;
};

/**
 * @brief Return whether the mesh crosshairs glyph should be visible
 * @param inputs Crosshairs glyph policy inputs
 * @return True when a nonzero glyph should be rendered
 */
bool shouldRenderMeshCrosshairsGlyph(const MeshCrosshairsGlyphInputs& inputs) noexcept;

/**
 * @brief Build the physical style used for mesh-rendered crosshair axes
 * @param inputs Crosshairs glyph policy inputs
 * @return Sphere glyph style with radius in world units
 */
MeshCrosshairsGlyphStyle meshCrosshairsGlyphStyle(const MeshCrosshairsGlyphInputs& inputs) noexcept;

} // namespace rendering::mesh
