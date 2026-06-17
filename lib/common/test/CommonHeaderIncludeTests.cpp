#include "common/Geometry.h"
#include "common/IntersectionTypes.h"

#include <catch2/catch_test_macros.hpp>

#include <glm/vec3.hpp>

#include <array>

TEST_CASE("common geometry public headers are self-contained", "[common][headers]")
{
  const auto box = math::computeAABBox<float>(std::array{glm::vec3{0.0f}, glm::vec3{2.0f}});

  CHECK(box.first == glm::vec3{0.0f});
  CHECK(box.second == glm::vec3{2.0f});
  CHECK(intersection::k_numIntersectionVertices == 7);
}
