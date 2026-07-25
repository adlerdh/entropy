#include "rendering/mesh/MeshPicking.h"

#include "rendering/mesh/MeshClipPlanes.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace rendering::mesh
{

std::optional<MeshPickRay> normalizedPickRay(const MeshPickRay& ray) noexcept
{
  const float length = glm::length(ray.direction);
  if (!std::isfinite(length) || length <= 0.0f) {
    return std::nullopt;
  }

  return MeshPickRay{ray.origin, ray.direction / length};
}

std::optional<float>
intersectRayAabb(const MeshPickRay& ray, const glm::vec3& minCorner, const glm::vec3& maxCorner) noexcept
{
  float nearDistance = 0.0f;
  float farDistance = std::numeric_limits<float>::infinity();

  for (int axis = 0; axis < 3; ++axis) {
    const float origin = ray.origin[axis];
    const float direction = ray.direction[axis];
    const float minValue = minCorner[axis];
    const float maxValue = maxCorner[axis];

    if (std::abs(direction) <= std::numeric_limits<float>::epsilon()) {
      if (origin < minValue || origin > maxValue) {
        return std::nullopt;
      }
      continue;
    }

    float t0 = (minValue - origin) / direction;
    float t1 = (maxValue - origin) / direction;
    if (t0 > t1) {
      std::swap(t0, t1);
    }

    nearDistance = std::max(nearDistance, t0);
    farDistance = std::min(farDistance, t1);
    if (nearDistance > farDistance) {
      return std::nullopt;
    }
  }

  return nearDistance;
}

std::optional<MeshTriangleHit>
intersectRayTriangle(const MeshPickRay& ray, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c) noexcept
{
  constexpr float epsilon = 1.0e-7f;

  const glm::vec3 edge1 = b - a;
  const glm::vec3 edge2 = c - a;
  const glm::vec3 p = glm::cross(ray.direction, edge2);
  const float determinant = glm::dot(edge1, p);
  if (std::abs(determinant) <= epsilon) {
    return std::nullopt;
  }

  const float inverseDeterminant = 1.0f / determinant;
  const glm::vec3 t = ray.origin - a;
  const float u = glm::dot(t, p) * inverseDeterminant;
  if (u < 0.0f || u > 1.0f) {
    return std::nullopt;
  }

  const glm::vec3 q = glm::cross(t, edge1);
  const float v = glm::dot(ray.direction, q) * inverseDeterminant;
  if (v < 0.0f || u + v > 1.0f) {
    return std::nullopt;
  }

  const float distance = glm::dot(edge2, q) * inverseDeterminant;
  if (distance < 0.0f || !std::isfinite(distance)) {
    return std::nullopt;
  }

  const glm::vec3 barycentric{1.0f - u - v, u, v};
  return MeshTriangleHit{distance, 0, ray.origin + distance * ray.direction, barycentric};
}

std::optional<MeshTriangleHit> pickNearestTriangle(
  const MeshData& mesh,
  const MeshPickRay& ray,
  const glm::mat4& world_T_mesh,
  std::span<const MeshClipPlane> clipPlanes)
{
  std::optional<MeshTriangleHit> nearestHit;

  for (size_t index = 0; index + 2 < mesh.indices.size(); index += 3) {
    const uint32_t ia = mesh.indices[index];
    const uint32_t ib = mesh.indices[index + 1];
    const uint32_t ic = mesh.indices[index + 2];
    if (ia >= mesh.positions.size() || ib >= mesh.positions.size() || ic >= mesh.positions.size()) {
      continue;
    }

    const glm::vec3 a = glm::vec3{world_T_mesh * glm::vec4{mesh.positions[ia], 1.0f}};
    const glm::vec3 b = glm::vec3{world_T_mesh * glm::vec4{mesh.positions[ib], 1.0f}};
    const glm::vec3 c = glm::vec3{world_T_mesh * glm::vec4{mesh.positions[ic], 1.0f}};

    std::optional<MeshTriangleHit> hit = intersectRayTriangle(ray, a, b, c);
    if (!hit || !pointInsideEnabledClipPlanes(hit->worldPosition, clipPlanes)) {
      continue;
    }

    hit->triangleIndex = static_cast<uint32_t>(index / 3);
    if (!nearestHit || hit->distance < nearestHit->distance) {
      nearestHit = hit;
    }
  }

  return nearestHit;
}

std::optional<MeshTriangleHit>
pickNearestTriangle(const MeshData& mesh, const MeshPickRay& ray, std::span<const MeshClipPlane> clipPlanes)
{
  return pickNearestTriangle(mesh, ray, glm::mat4{1.0f}, clipPlanes);
}

std::optional<MeshScenePickHit> pickNearestRenderable(const MeshScenePickRequest& request)
{
  if (!request.meshLookup) {
    return std::nullopt;
  }

  const std::optional<MeshPickRay> ray = normalizedPickRay(request.worldRay);
  if (!ray) {
    return std::nullopt;
  }

  std::optional<MeshScenePickHit> nearestHit;
  for (const MeshRenderable& renderable : request.renderables) {
    if (!renderable.visible || renderable.drawOptions.pickingMode == MeshPickingMode::Disabled) {
      continue;
    }

    const MeshData* mesh = request.meshLookup(renderable.mesh);
    if (!mesh) {
      continue;
    }

    const std::optional<MeshTriangleHit> hit =
      pickNearestTriangle(*mesh, *ray, renderable.world_T_mesh, renderable.drawOptions.clipPlanes);
    if (!hit) {
      continue;
    }

    if (!nearestHit || hit->distance < nearestHit->triangleHit.distance) {
      nearestHit = MeshScenePickHit{.mesh = renderable.mesh, .triangleHit = *hit};
    }
  }

  return nearestHit;
}

} // namespace rendering::mesh
