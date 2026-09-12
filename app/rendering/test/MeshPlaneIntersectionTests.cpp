#include "rendering/mesh/MeshPlaneIntersection.h"

#include "rendering/mesh/MeshData.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/common.hpp>
#include <glm/vec3.hpp>

#include <algorithm>
#include <limits>

namespace
{

rendering::mesh::MeshData unitCube()
{
  return rendering::mesh::MeshData{
    .positions =
      {{0.0f, 0.0f, 0.0f},
       {1.0f, 0.0f, 0.0f},
       {1.0f, 1.0f, 0.0f},
       {0.0f, 1.0f, 0.0f},
       {0.0f, 0.0f, 1.0f},
       {1.0f, 0.0f, 1.0f},
       {1.0f, 1.0f, 1.0f},
       {0.0f, 1.0f, 1.0f}},
    .indices = {0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7, 0, 1, 5, 0, 5, 4,
                1, 2, 6, 1, 6, 5, 2, 3, 7, 2, 7, 6, 3, 0, 4, 3, 4, 7}};
}

} // namespace

TEST_CASE("Mesh plane intersector cuts a triangle mesh and reuses its source", "[rendering][mesh][intersection]")
{
  auto created = rendering::mesh::MeshPlaneIntersector::create(unitCube());
  REQUIRE(created);
  auto intersector = std::move(*created);

  const auto middle = intersector.intersect({0.0f, 0.0f, 0.5f}, {0.0f, 0.0f, 3.0f});
  REQUIRE(middle);
  REQUIRE_FALSE(middle->empty());

  glm::vec3 minimum{std::numeric_limits<float>::max()};
  glm::vec3 maximum{std::numeric_limits<float>::lowest()};
  for (const auto& segment : *middle) {
    CHECK(segment.first.z == Catch::Approx(0.5f));
    CHECK(segment.second.z == Catch::Approx(0.5f));
    minimum = glm::min(minimum, glm::min(segment.first, segment.second));
    maximum = glm::max(maximum, glm::max(segment.first, segment.second));
  }
  CHECK(minimum.x == Catch::Approx(0.0f));
  CHECK(minimum.y == Catch::Approx(0.0f));
  CHECK(maximum.x == Catch::Approx(1.0f));
  CHECK(maximum.y == Catch::Approx(1.0f));

  const auto outside = intersector.intersect({0.0f, 0.0f, 2.0f}, {0.0f, 0.0f, 1.0f});
  REQUIRE(outside);
  CHECK(outside->empty());
}

TEST_CASE("Mesh plane intersector rejects invalid geometry and planes", "[rendering][mesh][intersection]")
{
  rendering::mesh::MeshData invalidMesh = unitCube();
  invalidMesh.indices.back() = 100;
  CHECK_FALSE(rendering::mesh::MeshPlaneIntersector::create(invalidMesh));

  auto created = rendering::mesh::MeshPlaneIntersector::create(unitCube());
  REQUIRE(created);
  auto intersector = std::move(*created);
  CHECK_FALSE(intersector.intersect({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}));
}
