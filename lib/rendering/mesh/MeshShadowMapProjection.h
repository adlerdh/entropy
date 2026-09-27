#pragma once

#include "rendering/mesh/MeshBounds.h"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <optional>

namespace rendering::mesh
{

/**
 * @brief Light-space transform and normalized direction for a shadow-map render pass
 */
struct MeshShadowMapProjection
{
  glm::mat4 lightClip_T_world = glm::mat4{1.0f}; //!< Transform from world coordinates to light clip coordinates
  glm::vec3 lightDirectionWorld = glm::vec3{0.0f, 0.0f, 1.0f}; //!< Direction from surface toward the light
};

/**
 * @brief Build a directional-light orthographic projection that encloses world-space bounds
 * @param bounds World-space scene bounds to enclose
 * @param lightDirectionWorld Direction from the surface toward the light
 * @return Projection, or empty when bounds or light direction are invalid
 */
std::optional<MeshShadowMapProjection> meshShadowMapProjectionForBounds(
  const MeshBounds& bounds,
  const glm::vec3& lightDirectionWorld) noexcept;

} // namespace rendering::mesh
