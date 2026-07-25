#pragma once

#include <glm/vec4.hpp>

namespace rendering::mesh
{

/**
 * @brief Mesh shading model selected by a renderable material
 */
enum class MeshShadingModel
{
  SimpleLit,
  PhysicallyBased
};

/**
 * @brief Surface appearance independent of mesh geometry
 *
 * Colors are normalized, non-premultiplied RGBA values. Changing these values must not invalidate CPU mesh geometry.
 */
struct MeshMaterial
{
  glm::vec4 baseColor = glm::vec4{0.8f, 0.8f, 0.8f, 1.0f};     //!< Normalized non-premultiplied RGBA
  float metallic = 0.0f;                                       //!< PBR metallic factor
  float roughness = 0.55f;                                     //!< PBR roughness factor
  float ambientOcclusion = 1.0f;                               //!< PBR ambient occlusion factor
  MeshShadingModel shadingModel = MeshShadingModel::SimpleLit; //!< Selected lighting model

  bool operator==(const MeshMaterial&) const = default;
};

/**
 * @brief Clamp and repair material values before shader upload
 * @param material Material to sanitize
 * @param fallbackColor Color used when the base color contains non-finite values
 * @return Shader-safe material values
 */
MeshMaterial sanitizedMaterial(const MeshMaterial& material, const glm::vec4& fallbackColor) noexcept;

} // namespace rendering::mesh
