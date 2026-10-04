#include "deformation/FieldTiles.h"

#include <catch2/catch_test_macros.hpp>

#include <glm/gtc/matrix_transform.hpp>

#include <vector>

namespace df = deformation;

TEST_CASE("Displaced tile dependencies preserve full-domain interpolation in 2D and 3D", "[deformation][tiles]")
{
  for (const bool volume : {false, true}) {
    df::DomainGeometry geometry;
    geometry.dimension = volume ? df::SpatialDimension::Volume : df::SpatialDimension::Plane;
    geometry.size = {10, 9, volume ? 6u : 1u};
    geometry.spacing = volume ? glm::dvec3{0.7, 1.1, 1.3} : glm::dvec3{0.7, 1.1, 1};
    geometry.origin = {100, -200, 300};
    geometry.directions = glm::dmat3(glm::rotate(glm::dmat4(1), 0.4, glm::dvec3(0, 0, 1)));
    const df::FieldDomain domain(geometry);
    std::vector<glm::vec4> inner(domain.sampleCount(), {0, 0, 0, 1});
    std::vector<glm::vec4> outer(domain.sampleCount(), {0.125f, 0.25f, 0, 1});
    const auto shift = domain.indexVectorToPhysical({2.25, -1.5, volume ? 0.5 : 0.0});
    for (auto& sample : inner)
      sample = glm::vec4(glm::vec3(shift), 1);
    const df::IndexExtent tile{{2, 3, volume ? 1u : 0u}, {5, 6, volume ? 4u : 1u}};
    const auto needed = df::compositionDependencies({domain, inner}, tile, domain.sampleCount());
    REQUIRE(needed);
    REQUIRE(needed->end[0] > tile.end[0]);
    REQUIRE(needed->begin[1] < tile.begin[1]);
    if (volume) REQUIRE(needed->end[2] > tile.end[2]);
    const auto fullOuter = outer;
    for (std::uint32_t z = 0; z < domain.size()[2]; ++z) {
      for (std::uint32_t y = 0; y < domain.size()[1]; ++y) {
        for (std::uint32_t x = 0; x < domain.size()[0]; ++x) {
          if (
            x < needed->begin[0] || x >= needed->end[0] || y < needed->begin[1] || y >= needed->end[1] ||
            z < needed->begin[2] || z >= needed->end[2])
            outer[(static_cast<std::size_t>(z) * domain.size()[1] + y) * domain.size()[0] + x].w = 0;
        }
      }
    }
    for (std::uint32_t z = tile.begin[2]; z < tile.end[2]; ++z) {
      for (std::uint32_t y = tile.begin[1]; y < tile.end[1]; ++y) {
        for (std::uint32_t x = tile.begin[0]; x < tile.end[0]; ++x) {
          const auto point = domain.indexToPhysical({x, y, z});
          const auto full = df::sampleDisplacement({domain, fullOuter}, point + glm::dvec3(shift));
          const auto cropped = df::sampleDisplacement({domain, outer}, point + glm::dvec3(shift));
          REQUIRE(cropped == full);
        }
      }
    }
    REQUIRE_THROWS(df::compositionDependencies({domain, inner}, tile, 1));
  }
}

TEST_CASE("Empty and malformed composition tiles fail without allocating dependencies", "[deformation][tiles]")
{
  df::DomainGeometry geometry;
  geometry.dimension = df::SpatialDimension::Plane;
  geometry.size = {4, 4, 1};
  const df::FieldDomain domain(geometry);
  std::vector<glm::vec4> invalid(domain.sampleCount(), glm::vec4(0));
  const df::IndexExtent tile{{1, 1, 0}, {3, 3, 1}};
  REQUIRE_FALSE(df::compositionDependencies({domain, invalid}, tile, 0));
  REQUIRE_THROWS(df::compositionDependencies({domain, invalid}, {{3, 1, 0}, {3, 3, 1}}, 100));
}
