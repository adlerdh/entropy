#pragma once

#include "rendering/mesh/MeshGlyphs.h"

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace rendering::mesh
{

/**
 * @brief Inputs needed to decide whether and how to draw one landmark glyph in mesh-rendered 3D views
 */
struct MeshLandmarkGlyphInputs
{
  bool groupVisible = true;                           //!< Landmark group visibility
  bool pointVisible = true;                           //!< Individual landmark visibility
  bool groupColorOverride = true;                     //!< True to use the group color instead of the point color
  float groupOpacity = 1.0f;                          //!< Landmark group opacity
  float radiusFactor = 0.02f;                         //!< Landmark radius scale factor
  float voxelDiagonalWorld = 1.0f;                    //!< Voxel diagonal length in world units
  glm::vec3 groupColor = glm::vec3{1.0f, 0.0f, 0.0f}; //!< Landmark group RGB color
  glm::vec3 pointColor = glm::vec3{0.5f};             //!< Individual landmark RGB color
};

/**
 * @brief Return whether a landmark glyph should be visible in the 3D mesh path
 * @param inputs Landmark glyph policy inputs
 * @return True when a nonzero glyph should be rendered
 */
bool shouldRenderMeshLandmarkGlyph(const MeshLandmarkGlyphInputs& inputs) noexcept;

/**
 * @brief Build the sphere-glyph style used for mesh-rendered landmarks
 * @param inputs Landmark glyph policy inputs
 * @return Sphere glyph style with radius in world units
 */
MeshSphereGlyphStyle meshLandmarkSphereGlyphStyle(const MeshLandmarkGlyphInputs& inputs) noexcept;

} // namespace rendering::mesh
