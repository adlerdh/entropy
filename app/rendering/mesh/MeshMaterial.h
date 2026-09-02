#pragma once

#include <glm/vec4.hpp>

namespace rendering::mesh
{

/**
 * @brief Mesh shading model selected by a renderable material
 */
enum class MeshShadingModel
{
  Unlit,
  SimpleLit,
  PhysicallyBased
};

/**
 * @brief Global material and silhouette-lighting settings shared by all rendered surfaces
 */
struct MeshSurfaceMaterialSettings
{
  bool flatShadingEnabled = false;  //!< Use one geometric normal per triangle instead of interpolated vertex normals
  bool pbrShadingEnabled = false;   //!< Use physically based shading instead of Blinn-Phong shading
  float metallic = 0.2f;            //!< PBR metallic factor
  float roughness = 0.3f;           //!< PBR roughness factor
  float ambientOcclusion = 1.0f;    //!< PBR indirect-light occlusion factor
  bool rimLightingEnabled = false;  //!< Enable view-angle rim opacity and glow
  float rimOpacityStrength = 1.0f;  //!< Strength of silhouette opacity modulation
  float rimEmissionStrength = 1.0f; //!< Strength of additive silhouette glow
  float rimPower = 2.0f;            //!< Rim falloff exponent

  bool operator==(const MeshSurfaceMaterialSettings&) const = default;
};

/**
 * @brief Surface appearance independent of mesh geometry
 *
 * Colors are normalized, non-premultiplied RGBA values. Changing these values must not invalidate CPU mesh geometry.
 */
struct MeshMaterial
{
  glm::vec4 baseColor = glm::vec4{0.8f, 0.8f, 0.8f, 1.0f};     //!< Normalized non-premultiplied RGBA
  float metallic = 0.2f;                                       //!< PBR metallic factor
  float roughness = 0.3f;                                      //!< PBR roughness factor
  float ambientOcclusion = 1.0f;                               //!< PBR ambient occlusion factor
  MeshShadingModel shadingModel = MeshShadingModel::SimpleLit; //!< Selected lighting model
  bool rimLightingEnabled = false;                             //!< Enable view-angle rim opacity and glow
  float rimOpacityStrength = 1.0f;                             //!< Strength of silhouette opacity modulation
  float rimEmissionStrength = 1.0f;                            //!< Strength of additive silhouette glow
  float rimPower = 2.0f;                                       //!< Rim falloff exponent

  bool operator==(const MeshMaterial&) const = default;
};

/**
 * @brief Clamp and repair material values before shader upload
 * @param material Material to sanitize
 * @param fallbackColor Color used when the base color contains non-finite values
 * @return Shader-safe material values
 */
MeshMaterial sanitizedMaterial(const MeshMaterial& material, const glm::vec4& fallbackColor) noexcept;

/**
 * @brief Create a renderable material from a surface color and the global surface settings
 */
MeshMaterial meshMaterialForSurface(const glm::vec4& baseColor, const MeshSurfaceMaterialSettings& settings) noexcept;

} // namespace rendering::mesh
