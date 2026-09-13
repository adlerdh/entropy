#pragma once

#include <glm/mat4x4.hpp>

#include <array>

namespace rendering::mesh
{

/**
 * @brief Inputs needed to decide whether and how to draw the crosshairs glyph in mesh-rendered 3D views
 */
struct MeshCrosshairsGlyphInputs
{
  bool showCrosshairsIn3D = true;       //!< User-facing 3D crosshairs glyph visibility setting
  bool cameraFollowsCrosshairs = false; //!< Whether the 3D camera eye follows the crosshairs position
  float diameterScenePercent = 0.25f;   //!< Glyph diameter as a percentage of the visible scene diagonal
  float lengthScenePercent = 4.0f;      //!< Per-axis glyph length as a percentage of the visible scene diagonal
  float sceneDiagonalWorld = 1.0f;      //!< Visible scene bounding-box diagonal in world units
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
 * @return Crosshairs glyph style with radius and half-length in world units
 */
MeshCrosshairsGlyphStyle meshCrosshairsGlyphStyle(const MeshCrosshairsGlyphInputs& inputs) noexcept;

/** Build the three scaled axis transforms from the complete rotated crosshairs frame. */
std::array<glm::mat4, 3> meshCrosshairsAxisWorldTransforms(
  const glm::mat4& world_T_crosshairs,
  const MeshCrosshairsGlyphStyle& style) noexcept;

} // namespace rendering::mesh
