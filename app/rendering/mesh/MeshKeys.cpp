#include "rendering/mesh/MeshKeys.h"

#include <functional>

namespace rendering::mesh
{

namespace
{

template<typename Value>
void hashCombine(std::size_t& seed, const Value& value)
{
  seed ^= std::hash<Value>{}(value) + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
}

template<typename Value>
void hashOptional(std::size_t& seed, const std::optional<Value>& value)
{
  hashCombine(seed, value.has_value());
  if (value) {
    hashCombine(seed, *value);
  }
}

void hashVec4(std::size_t& seed, const glm::vec4& value)
{
  hashCombine(seed, value.x);
  hashCombine(seed, value.y);
  hashCombine(seed, value.z);
  hashCombine(seed, value.w);
}

void hashVec3(std::size_t& seed, const glm::vec3& value)
{
  hashCombine(seed, value.x);
  hashCombine(seed, value.y);
  hashCombine(seed, value.z);
}

} // namespace

std::size_t MeshGeometryKeyHash::operator()(const MeshGeometryKey& key) const
{
  std::size_t seed = 0;
  hashCombine(seed, key.sourceUid);
  hashCombine(seed, key.sourceDataVersion);
  hashCombine(seed, key.sourceGeometryVersion);
  hashOptional(seed, key.component);
  hashOptional(seed, key.labelValue);
  hashCombine(seed, key.timePoint);
  hashCombine(seed, key.isoValue);
  hashCombine(seed, key.extractionAlgorithm);
  hashCombine(seed, key.extractionAlgorithmVersion);
  return seed;
}

std::size_t MeshStyleKeyHash::operator()(const MeshStyleKey& key) const
{
  std::size_t seed = 0;
  hashVec4(seed, key.material.baseColor);
  hashCombine(seed, key.material.metallic);
  hashCombine(seed, key.material.roughness);
  hashCombine(seed, key.material.ambientOcclusion);
  hashCombine(seed, static_cast<int>(key.material.shadingModel));
  hashCombine(seed, key.material.flatShadingEnabled);
  hashCombine(seed, key.material.triangleEdgesEnabled);
  hashVec3(seed, key.material.triangleEdgeColor);
  hashCombine(seed, key.material.rimLightingEnabled);
  hashCombine(seed, key.material.rimOpacityStrength);
  hashCombine(seed, key.material.rimEmissionStrength);
  hashCombine(seed, key.material.rimPower);
  hashCombine(seed, static_cast<int>(key.compositingMode));
  hashCombine(seed, static_cast<int>(key.fillMode));
  hashCombine(seed, key.backfaceCulling);
  return seed;
}

} // namespace rendering::mesh
