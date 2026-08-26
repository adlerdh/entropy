#include "rendering/mesh/MeshImagePlaneScene.h"

#include "rendering/mesh/MeshImagePlane.h"
#include "rendering/utility/math/SliceIntersector.h"

#include <glm/vec4.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>

namespace rendering::mesh
{

namespace
{

template<typename Value>
void hashCombine(std::size_t& seed, const Value& value) noexcept
{
  seed ^= std::hash<Value>{}(value) + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
}

void hashVec3(std::size_t& seed, const glm::vec3& value) noexcept
{
  hashCombine(seed, value.x);
  hashCombine(seed, value.y);
  hashCombine(seed, value.z);
}

void hashMat4(std::size_t& seed, const glm::mat4& value) noexcept
{
  for (glm::length_t column = 0; column < 4; ++column) {
    for (glm::length_t row = 0; row < 4; ++row) {
      hashCombine(seed, value[column][row]);
    }
  }
}

intersection::AlignmentMethod alignmentForOrientation(const MeshImagePlaneOrientation orientation) noexcept
{
  switch (orientation) {
    case MeshImagePlaneOrientation::Axial:
      return intersection::AlignmentMethod::FrameZ;
    case MeshImagePlaneOrientation::Coronal:
      return intersection::AlignmentMethod::FrameY;
    case MeshImagePlaneOrientation::Sagittal:
      return intersection::AlignmentMethod::FrameX;
  }

  return intersection::AlignmentMethod::FrameZ;
}

std::optional<glm::vec3> normalizedVec3(const glm::vec3& value) noexcept
{
  const float length = glm::length(value);
  if (!std::isfinite(length) || length <= 0.0f) {
    return std::nullopt;
  }
  return value / length;
}

std::optional<intersection::IntersectionVerticesVec4> worldIntersectionsForPlane(
  const MeshImagePlaneSceneInputs& inputs,
  const MeshImagePlaneOrientation orientation)
{
  SliceIntersector intersector;
  intersector.setPositioningMethod(intersection::PositioningMethod::FrameOrigin);
  intersector.setAlignmentMethod(alignmentForOrientation(orientation));

  auto [pixelIntersections, unusedPlane] = intersector.computePlaneIntersections(
    glm::mat4{1.0f},
    inputs.pixel_T_world * inputs.world_T_crosshairs,
    inputs.pixelBoxCorners);
  (void)unusedPlane;

  if (!pixelIntersections) {
    return std::nullopt;
  }

  const glm::vec3 planeOrigin = glm::vec3{inputs.world_T_crosshairs[3]};
  const glm::vec3 planeNormal = imagePlaneWorldNormal(orientation, inputs.world_T_crosshairs);
  intersection::IntersectionVerticesVec4 worldIntersections{};
  for (std::size_t i = 0; i < worldIntersections.size(); ++i) {
    worldIntersections[i] = inputs.world_T_pixel * glm::vec4{pixelIntersections->at(i), 1.0f};
    // Every image's polygon for a given orientation represents the same world-space crosshair plane. Remove
    // image-transform round-trip error so coincident planes remain exactly coplanar at grazing view angles.
    glm::vec3 worldPosition = glm::vec3{worldIntersections[i]} / worldIntersections[i].w;
    worldPosition -= planeNormal * glm::dot(planeNormal, worldPosition - planeOrigin);
    worldIntersections[i] = glm::vec4{worldPosition, 1.0f};
  }
  return worldIntersections;
}

} // namespace

glm::vec3 imagePlaneWorldNormal(
  const MeshImagePlaneOrientation orientation,
  const glm::mat4& world_T_crosshairs) noexcept
{
  glm::vec3 frameNormal{0.0f, 0.0f, 1.0f};
  switch (orientation) {
    case MeshImagePlaneOrientation::Axial:
      frameNormal = glm::vec3{0.0f, 0.0f, 1.0f};
      break;
    case MeshImagePlaneOrientation::Coronal:
      frameNormal = glm::vec3{0.0f, 1.0f, 0.0f};
      break;
    case MeshImagePlaneOrientation::Sagittal:
      frameNormal = glm::vec3{1.0f, 0.0f, 0.0f};
      break;
  }
  const std::optional<glm::vec3> transformed = normalizedVec3(glm::mat3{world_T_crosshairs} * frameNormal);
  return transformed.value_or(frameNormal);
}

float imagePlaneViewOpacityMultiplier(const glm::vec3& planeNormalWorld, const glm::vec3& viewDirectionWorld) noexcept
{
  const std::optional<glm::vec3> normal = normalizedVec3(planeNormalWorld);
  const std::optional<glm::vec3> viewDirection = normalizedVec3(viewDirectionWorld);
  if (!normal || !viewDirection) {
    return 0.0f;
  }

  return std::clamp(std::abs(glm::dot(*normal, *viewDirection)), 0.0f, 1.0f);
}

std::uint64_t imagePlaneSceneGeometryVersion(
  const MeshImagePlaneSceneInputs& inputs,
  const MeshImagePlaneOrientation orientation) noexcept
{
  std::size_t seed = 0;
  hashCombine(seed, static_cast<int>(orientation));
  hashMat4(seed, inputs.world_T_crosshairs);
  hashMat4(seed, inputs.world_T_pixel);
  hashMat4(seed, inputs.pixel_T_world);
  hashMat4(seed, inputs.texture_T_world);
  for (const glm::vec3& corner : inputs.pixelBoxCorners) {
    hashVec3(seed, corner);
  }
  hashCombine(seed, inputs.borderWidthWorld);
  return static_cast<std::uint64_t>(seed);
}

std::uint64_t imageBoxSceneGeometryVersion(
  const std::array<glm::vec3, 8>& worldCorners,
  const float borderWidthWorld) noexcept
{
  std::size_t seed = 0;
  hashCombine(seed, 0x4b7d2a31u);
  for (const glm::vec3& corner : worldCorners) {
    hashVec3(seed, corner);
  }
  hashCombine(seed, borderWidthWorld);
  return static_cast<std::uint64_t>(seed);
}

std::vector<MeshImagePlaneSceneMesh> buildOrthogonalImagePlaneSceneMeshes(const MeshImagePlaneSceneInputs& inputs)
{
  std::vector<MeshImagePlaneSceneMesh> meshes;
  meshes.reserve(inputs.orientations.size());

  for (const MeshImagePlaneOrientation orientation : inputs.orientations) {
    const std::optional<intersection::IntersectionVerticesVec4> intersections =
      worldIntersectionsForPlane(inputs, orientation);
    if (!intersections) {
      continue;
    }

    std::optional<MeshData> mesh = makeTexturedImageSliceIntersectionMesh(*intersections, inputs.texture_T_world);
    if (!mesh) {
      continue;
    }

    std::optional<MeshData> borderMesh;
    if (inputs.borderWidthWorld > 0.0f) {
      borderMesh = makeImageSliceIntersectionBorderMesh(*intersections, inputs.borderWidthWorld);
    }

    meshes.push_back(MeshImagePlaneSceneMesh{
      .orientation = orientation,
      .mesh = std::move(*mesh),
      .borderMesh = std::move(borderMesh)});
  }

  return meshes;
}

} // namespace rendering::mesh
