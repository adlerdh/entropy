#include "rendering/mesh/MeshImagePlane.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

namespace rendering::mesh
{

namespace
{

std::optional<glm::vec3> normalizedDirection(const glm::vec3& direction) noexcept
{
  const float length = glm::length(direction);
  if (!std::isfinite(length) || length <= 0.0f) {
    return std::nullopt;
  }

  return direction / length;
}

bool nearlyEqual(const glm::vec3& a, const glm::vec3& b) noexcept
{
  static constexpr float k_epsilon = 1.0e-5f;
  return glm::all(glm::lessThanEqual(glm::abs(a - b), glm::vec3{k_epsilon}));
}

bool isFiniteVec3(const glm::vec3& value) noexcept
{
  return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

std::vector<glm::vec3> uniquePolygonVertices(const intersection::IntersectionVertices& intersections)
{
  std::vector<glm::vec3> vertices;
  vertices.reserve(intersection::k_numIntersectionVertices - 1);

  for (std::size_t i = 0; i < intersection::k_numIntersectionVertices - 1; ++i) {
    const glm::vec3& vertex = intersections[i];
    const bool alreadyPresent =
      std::ranges::any_of(vertices, [&vertex](const glm::vec3& existing) { return nearlyEqual(vertex, existing); });
    if (!alreadyPresent) {
      vertices.push_back(vertex);
    }
  }

  return vertices;
}

std::optional<glm::vec3> polygonNormal(const std::vector<glm::vec3>& vertices) noexcept
{
  if (vertices.size() < 3) {
    return std::nullopt;
  }

  glm::vec3 normal{0.0f};
  for (std::size_t i = 0; i < vertices.size(); ++i) {
    const glm::vec3& current = vertices[i];
    const glm::vec3& next = vertices[(i + 1u) % vertices.size()];
    normal.x += (current.y - next.y) * (current.z + next.z);
    normal.y += (current.z - next.z) * (current.x + next.x);
    normal.z += (current.x - next.x) * (current.y + next.y);
  }

  return normalizedDirection(normal);
}

} // namespace

MeshImagePlane makeAxialImagePlane(const glm::vec3& centerWorld, const glm::vec2& sizeWorld) noexcept
{
  return MeshImagePlane{
    .centerWorld = centerWorld,
    .uDirectionWorld = glm::vec3{-1.0f, 0.0f, 0.0f},
    .vDirectionWorld = glm::vec3{0.0f, 1.0f, 0.0f},
    .sizeWorld = sizeWorld};
}

MeshImagePlane makeCoronalImagePlane(const glm::vec3& centerWorld, const glm::vec2& sizeWorld) noexcept
{
  return MeshImagePlane{
    .centerWorld = centerWorld,
    .uDirectionWorld = glm::vec3{-1.0f, 0.0f, 0.0f},
    .vDirectionWorld = glm::vec3{0.0f, 0.0f, 1.0f},
    .sizeWorld = sizeWorld};
}

MeshImagePlane makeSagittalImagePlane(const glm::vec3& centerWorld, const glm::vec2& sizeWorld) noexcept
{
  return MeshImagePlane{
    .centerWorld = centerWorld,
    .uDirectionWorld = glm::vec3{0.0f, -1.0f, 0.0f},
    .vDirectionWorld = glm::vec3{0.0f, 0.0f, 1.0f},
    .sizeWorld = sizeWorld};
}

std::optional<MeshData> makeImagePlaneMesh(const MeshImagePlane& plane)
{
  if (
    plane.sizeWorld.x <= 0.0f || plane.sizeWorld.y <= 0.0f || !std::isfinite(plane.sizeWorld.x) ||
    !std::isfinite(plane.sizeWorld.y))
  {
    return std::nullopt;
  }

  const std::optional<glm::vec3> u = normalizedDirection(plane.uDirectionWorld);
  if (!u) {
    return std::nullopt;
  }

  const glm::vec3 vOrthogonal = plane.vDirectionWorld - glm::dot(plane.vDirectionWorld, *u) * *u;
  const std::optional<glm::vec3> v = normalizedDirection(vOrthogonal);
  if (!v) {
    return std::nullopt;
  }

  const glm::vec3 normal = glm::normalize(glm::cross(*u, *v));
  const glm::vec3 halfU = 0.5f * plane.sizeWorld.x * *u;
  const glm::vec3 halfV = 0.5f * plane.sizeWorld.y * *v;

  MeshData mesh;
  mesh.coordinateSpace = MeshCoordinateSpace::World;
  mesh.positions = {
    plane.centerWorld - halfU - halfV,
    plane.centerWorld + halfU - halfV,
    plane.centerWorld + halfU + halfV,
    plane.centerWorld - halfU + halfV};
  mesh.normals = {normal, normal, normal, normal};
  mesh.indices = {0, 1, 2, 0, 2, 3};
  return mesh;
}

std::optional<MeshData> withImageTextureCoordinates(const MeshData& mesh, const glm::mat4& texture_T_world)
{
  MeshData texturedMesh = mesh;
  std::vector<glm::vec3> textureCoords;
  textureCoords.reserve(mesh.positions.size());

  for (const glm::vec3& position : mesh.positions) {
    const glm::vec4 textureCoordH = texture_T_world * glm::vec4{position, 1.0f};
    if (std::abs(textureCoordH.w) <= 1.0e-6f) {
      return std::nullopt;
    }

    const glm::vec3 textureCoord{textureCoordH / textureCoordH.w};
    if (!isFiniteVec3(textureCoord)) {
      return std::nullopt;
    }

    textureCoords.push_back(textureCoord);
  }

  texturedMesh.textureCoords = std::move(textureCoords);
  return texturedMesh;
}

std::optional<MeshData> makeTexturedImagePlaneMesh(const MeshImagePlane& plane, const glm::mat4& texture_T_world)
{
  const std::optional<MeshData> mesh = makeImagePlaneMesh(plane);
  return mesh ? withImageTextureCoordinates(*mesh, texture_T_world) : std::nullopt;
}

std::optional<MeshData> makeImageSliceIntersectionMesh(const intersection::IntersectionVertices& intersections)
{
  const std::vector<glm::vec3> polygon = uniquePolygonVertices(intersections);
  const std::optional<glm::vec3> normal = polygonNormal(polygon);
  if (!normal) {
    return std::nullopt;
  }

  MeshData mesh;
  mesh.coordinateSpace = MeshCoordinateSpace::World;
  mesh.positions.reserve(polygon.size() + 1u);
  mesh.positions.push_back(intersections.back());
  mesh.positions.insert(mesh.positions.end(), polygon.begin(), polygon.end());
  mesh.normals.assign(mesh.positions.size(), *normal);
  mesh.indices.reserve(3u * polygon.size());

  // The slice intersector stores vertices in polygon order. Add one center vertex and fan triangles around it.
  for (std::uint32_t i = 0; i < polygon.size(); ++i) {
    const std::uint32_t next = (i + 1u) % static_cast<std::uint32_t>(polygon.size());
    mesh.indices.push_back(0u);
    mesh.indices.push_back(i + 1u);
    mesh.indices.push_back(next + 1u);
  }

  return mesh;
}

std::optional<MeshData> makeImageSliceIntersectionMesh(const intersection::IntersectionVerticesVec4& intersections)
{
  intersection::IntersectionVertices cartesian{};
  for (std::size_t i = 0; i < intersections.size(); ++i) {
    const glm::vec4& point = intersections[i];
    if (std::abs(point.w) <= 1.0e-6f) {
      return std::nullopt;
    }
    cartesian[i] = glm::vec3{point / point.w};
  }

  return makeImageSliceIntersectionMesh(cartesian);
}

std::optional<MeshData> makeTexturedImageSliceIntersectionMesh(
  const intersection::IntersectionVertices& intersections,
  const glm::mat4& texture_T_world)
{
  const std::optional<MeshData> mesh = makeImageSliceIntersectionMesh(intersections);
  return mesh ? withImageTextureCoordinates(*mesh, texture_T_world) : std::nullopt;
}

std::optional<MeshData> makeTexturedImageSliceIntersectionMesh(
  const intersection::IntersectionVerticesVec4& intersections,
  const glm::mat4& texture_T_world)
{
  const std::optional<MeshData> mesh = makeImageSliceIntersectionMesh(intersections);
  return mesh ? withImageTextureCoordinates(*mesh, texture_T_world) : std::nullopt;
}

} // namespace rendering::mesh
