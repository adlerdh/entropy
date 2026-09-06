#include "rendering/mesh/MeshPrimitives.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace rendering::mesh
{

MeshData makeCubeMesh(float edgeLength)
{
  const float half = std::max(edgeLength, 0.0f) * 0.5f;

  const std::array faceNormals{
    glm::vec3{1.0f, 0.0f, 0.0f},
    glm::vec3{-1.0f, 0.0f, 0.0f},
    glm::vec3{0.0f, 1.0f, 0.0f},
    glm::vec3{0.0f, -1.0f, 0.0f},
    glm::vec3{0.0f, 0.0f, 1.0f},
    glm::vec3{0.0f, 0.0f, -1.0f}};

  const std::array<std::array<glm::vec3, 4>, 6> facePositions{
    {{glm::vec3{half, -half, -half},
      glm::vec3{half, half, -half},
      glm::vec3{half, half, half},
      glm::vec3{half, -half, half}},
     {glm::vec3{-half, half, -half},
      glm::vec3{-half, -half, -half},
      glm::vec3{-half, -half, half},
      glm::vec3{-half, half, half}},
     {glm::vec3{-half, half, -half},
      glm::vec3{half, half, -half},
      glm::vec3{half, half, half},
      glm::vec3{-half, half, half}},
     {glm::vec3{half, -half, -half},
      glm::vec3{-half, -half, -half},
      glm::vec3{-half, -half, half},
      glm::vec3{half, -half, half}},
     {glm::vec3{-half, -half, half},
      glm::vec3{half, -half, half},
      glm::vec3{half, half, half},
      glm::vec3{-half, half, half}},
     {glm::vec3{-half, half, -half},
      glm::vec3{half, half, -half},
      glm::vec3{half, -half, -half},
      glm::vec3{-half, -half, -half}}}};

  MeshData mesh;
  mesh.positions.reserve(24);
  mesh.normals.reserve(24);
  mesh.indices.reserve(36);
  mesh.coordinateSpace = MeshCoordinateSpace::World;

  for (size_t face = 0; face < facePositions.size(); ++face) {
    const uint32_t baseIndex = static_cast<uint32_t>(mesh.positions.size());
    for (const glm::vec3& position : facePositions[face]) {
      mesh.positions.push_back(position);
      mesh.normals.push_back(faceNormals[face]);
    }

    mesh.indices.insert(
      mesh.indices.end(),
      {baseIndex, baseIndex + 1u, baseIndex + 2u, baseIndex, baseIndex + 2u, baseIndex + 3u});
  }

  return mesh;
}

MeshData makeSphereMesh(const float radius, const uint32_t rings, const uint32_t segments)
{
  const float r = std::max(radius, 0.0f);
  const uint32_t ringCount = std::max(rings, 1u);
  const uint32_t segmentCount = std::max(segments, 3u);

  MeshData mesh;
  mesh.coordinateSpace = MeshCoordinateSpace::World;
  mesh.positions.reserve(static_cast<size_t>(ringCount + 1u) * static_cast<size_t>(segmentCount) + 2u);
  mesh.normals.reserve(mesh.positions.capacity());
  mesh.indices.reserve(static_cast<size_t>(segmentCount) * static_cast<size_t>(ringCount) * 6u);

  const uint32_t northPole = static_cast<uint32_t>(mesh.positions.size());
  mesh.positions.emplace_back(0.0f, 0.0f, r);
  mesh.normals.emplace_back(0.0f, 0.0f, 1.0f);

  for (uint32_t ring = 1; ring <= ringCount; ++ring) {
    const float theta = std::numbers::pi_v<float> * static_cast<float>(ring) / static_cast<float>(ringCount + 1u);
    const float z = std::cos(theta);
    const float radial = std::sin(theta);

    for (uint32_t segment = 0; segment < segmentCount; ++segment) {
      const float phi =
        2.0f * std::numbers::pi_v<float> * static_cast<float>(segment) / static_cast<float>(segmentCount);
      const glm::vec3 normal{radial * std::cos(phi), radial * std::sin(phi), z};
      mesh.positions.emplace_back(r * normal);
      mesh.normals.emplace_back(glm::normalize(normal));
    }
  }

  const uint32_t southPole = static_cast<uint32_t>(mesh.positions.size());
  mesh.positions.emplace_back(0.0f, 0.0f, -r);
  mesh.normals.emplace_back(0.0f, 0.0f, -1.0f);

  const auto ringVertex = [segmentCount](const uint32_t ring, const uint32_t segment) -> uint32_t {
    return 1u + (ring - 1u) * segmentCount + (segment % segmentCount);
  };

  for (uint32_t segment = 0; segment < segmentCount; ++segment) {
    mesh.indices.insert(mesh.indices.end(), {northPole, ringVertex(1, segment), ringVertex(1, segment + 1u)});
  }

  for (uint32_t ring = 1; ring < ringCount; ++ring) {
    for (uint32_t segment = 0; segment < segmentCount; ++segment) {
      const uint32_t a = ringVertex(ring, segment);
      const uint32_t b = ringVertex(ring, segment + 1u);
      const uint32_t c = ringVertex(ring + 1u, segment + 1u);
      const uint32_t d = ringVertex(ring + 1u, segment);
      mesh.indices.insert(mesh.indices.end(), {a, d, c, a, c, b});
    }
  }

  for (uint32_t segment = 0; segment < segmentCount; ++segment) {
    mesh.indices.insert(
      mesh.indices.end(),
      {ringVertex(ringCount, segment), southPole, ringVertex(ringCount, segment + 1u)});
  }

  return mesh;
}

MeshData makeCylinderMesh(const float radius, const float height, const uint32_t segments)
{
  const float r = std::max(radius, 0.0f);
  const float halfHeight = std::max(height, 0.0f) * 0.5f;
  const uint32_t segmentCount = std::max(segments, 3u);

  MeshData mesh;
  mesh.coordinateSpace = MeshCoordinateSpace::World;
  mesh.positions.reserve(static_cast<size_t>(segmentCount) * 4u + 2u);
  mesh.normals.reserve(mesh.positions.capacity());
  mesh.indices.reserve(static_cast<size_t>(segmentCount) * 12u);

  for (uint32_t segment = 0; segment < segmentCount; ++segment) {
    const float phi = 2.0f * std::numbers::pi_v<float> * static_cast<float>(segment) / static_cast<float>(segmentCount);
    const glm::vec3 normal{std::cos(phi), std::sin(phi), 0.0f};
    mesh.positions.emplace_back(r * normal.x, r * normal.y, -halfHeight);
    mesh.positions.emplace_back(r * normal.x, r * normal.y, halfHeight);
    mesh.normals.emplace_back(normal);
    mesh.normals.emplace_back(normal);
  }

  const uint32_t topCenter = static_cast<uint32_t>(mesh.positions.size());
  mesh.positions.emplace_back(0.0f, 0.0f, halfHeight);
  mesh.normals.emplace_back(0.0f, 0.0f, 1.0f);
  const uint32_t bottomCenter = static_cast<uint32_t>(mesh.positions.size());
  mesh.positions.emplace_back(0.0f, 0.0f, -halfHeight);
  mesh.normals.emplace_back(0.0f, 0.0f, -1.0f);

  const uint32_t capStart = static_cast<uint32_t>(mesh.positions.size());
  for (uint32_t segment = 0; segment < segmentCount; ++segment) {
    const float phi = 2.0f * std::numbers::pi_v<float> * static_cast<float>(segment) / static_cast<float>(segmentCount);
    const float x = r * std::cos(phi);
    const float y = r * std::sin(phi);
    mesh.positions.emplace_back(x, y, halfHeight);
    mesh.positions.emplace_back(x, y, -halfHeight);
    mesh.normals.emplace_back(0.0f, 0.0f, 1.0f);
    mesh.normals.emplace_back(0.0f, 0.0f, -1.0f);
  }

  for (uint32_t segment = 0; segment < segmentCount; ++segment) {
    const uint32_t next = (segment + 1u) % segmentCount;
    const uint32_t bottom0 = 2u * segment;
    const uint32_t top0 = bottom0 + 1u;
    const uint32_t bottom1 = 2u * next;
    const uint32_t top1 = bottom1 + 1u;
    mesh.indices.insert(mesh.indices.end(), {bottom0, bottom1, top1, bottom0, top1, top0});

    const uint32_t capTop0 = capStart + 2u * segment;
    const uint32_t capBottom0 = capTop0 + 1u;
    const uint32_t capTop1 = capStart + 2u * next;
    const uint32_t capBottom1 = capTop1 + 1u;
    mesh.indices.insert(mesh.indices.end(), {topCenter, capTop0, capTop1, bottomCenter, capBottom1, capBottom0});
  }

  return mesh;
}

} // namespace rendering::mesh
