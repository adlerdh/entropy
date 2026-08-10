#include "rendering/mesh/MeshImagePlaneScene.h"

#include "common/CoordinateFrame.h"
#include "rendering/mesh/MeshImagePlane.h"
#include "rendering/utility/math/SliceIntersector.h"

#include <glm/vec4.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace rendering::mesh
{

namespace
{

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
  const CoordinateFrame worldFrame{inputs.worldCrosshairs, glm::quat{1.0f, 0.0f, 0.0f, 0.0f}};
  SliceIntersector intersector;
  intersector.setPositioningMethod(intersection::PositioningMethod::FrameOrigin);
  intersector.setAlignmentMethod(alignmentForOrientation(orientation));

  auto [pixelIntersections, unusedPlane] = intersector.computePlaneIntersections(
    glm::mat4{1.0f},
    inputs.pixel_T_world * worldFrame.world_T_frame(),
    inputs.pixelBoxCorners);
  (void)unusedPlane;

  if (!pixelIntersections) {
    return std::nullopt;
  }

  intersection::IntersectionVerticesVec4 worldIntersections{};
  for (std::size_t i = 0; i < worldIntersections.size(); ++i) {
    worldIntersections[i] = inputs.world_T_pixel * glm::vec4{pixelIntersections->at(i), 1.0f};
  }
  return worldIntersections;
}

} // namespace

glm::vec3 imagePlaneWorldNormal(const MeshImagePlaneOrientation orientation) noexcept
{
  switch (orientation) {
    case MeshImagePlaneOrientation::Axial:
      return glm::vec3{0.0f, 0.0f, 1.0f};
    case MeshImagePlaneOrientation::Coronal:
      return glm::vec3{0.0f, 1.0f, 0.0f};
    case MeshImagePlaneOrientation::Sagittal:
      return glm::vec3{1.0f, 0.0f, 0.0f};
  }

  return glm::vec3{0.0f, 0.0f, 1.0f};
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
