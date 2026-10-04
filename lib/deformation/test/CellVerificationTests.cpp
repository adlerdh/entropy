#include "deformation/CellVerification.h"

#include <catch2/catch_test_macros.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace df = deformation;

namespace
{
df::FieldDomain domain(bool volume, std::uint32_t side = 5)
{
  df::DomainGeometry spec;
  spec.dimension = volume ? df::SpatialDimension::Volume : df::SpatialDimension::Plane;
  spec.size = {side, side, volume ? side : 1};
  spec.spacing = {0.5, 1.25, volume ? 2.0 : 1.0};
  spec.origin = {1e6, -2e6, 3e6};
  spec.directions = glm::dmat3(glm::rotate(glm::dmat4(1), 0.4, glm::normalize(glm::dvec3(1, 2, 3))));
  spec.directions[0] *= -1;
  return df::FieldDomain(spec);
}
std::size_t offset(const df::FieldDomain& field, std::uint32_t x, std::uint32_t y, std::uint32_t z)
{
  return (static_cast<std::size_t>(z) * field.size()[1] + y) * field.size()[0] + x;
}
} // namespace

TEST_CASE("Cell verification covers the complete represented identity in oblique 2D and 3D", "[deformation][quality]")
{
  for (const bool volume : {false, true}) {
    const auto field = domain(volume);
    const std::vector<glm::vec4> values(field.sampleCount(), {0, 0, 0, 1});
    const auto checked = df::verifyFieldCells(field, values, {});
    REQUIRE(checked.requested == (volume ? 64 : 16));
    REQUIRE(checked.verified == checked.requested);
    REQUIRE(checked.complete());
    REQUIRE_FALSE(df::verifyFieldCells(field, values, {}, 3, field.sampleCount(), 0).complete());
  }
}

TEST_CASE("Cell verification distinguishes folds missing values and unresolved compression", "[deformation][quality]")
{
  for (const bool volume : {false, true}) {
    const auto field = domain(volume);
    std::vector<glm::vec4> values(field.sampleCount(), {0, 0, 0, 1});
    for (std::uint32_t z = 0; z < field.size()[2]; ++z) {
      for (std::uint32_t y = 0; y < field.size()[1]; ++y) {
        for (std::uint32_t x = 0; x < field.size()[0]; ++x) {
          const auto physical = field.indexVectorToPhysical({x, 0, 0});
          values[offset(field, x, y, z)] = glm::vec4(-2.0 * physical, 1);
        }
      }
    }
    const auto folded = df::verifyFieldCells(field, values, {});
    REQUIRE(folded.folded == folded.requested);
    REQUIRE_FALSE(folded.complete());
    values[offset(field, 2, 2, 0)].x = std::numeric_limits<float>::quiet_NaN();
    REQUIRE(df::verifyFieldCells(field, values, {}).invalid > 0);
    values.assign(field.sampleCount(), {0, 0, 0, 1});
    for (std::uint32_t z = 0; z < field.size()[2]; ++z) {
      for (std::uint32_t y = 0; y < field.size()[1]; ++y) {
        for (std::uint32_t x = 0; x < field.size()[0]; ++x) {
          const auto displacement = -0.98 * field.indexVectorToPhysical({x, 0, 0});
          values[offset(field, x, y, z)] = glm::vec4(displacement, 1);
        }
      }
    }
    const auto compressed = df::verifyFieldCells(field, values, {});
    REQUIRE(compressed.unresolved == compressed.requested);
    REQUIRE_FALSE(compressed.complete());
  }
}

TEST_CASE("Cell verification rejects unsupported inputs and singleton volumes", "[deformation][quality]")
{
  const auto plane = domain(false);
  REQUIRE_THROWS_AS(df::verifyFieldCells(plane, {}, {}), std::invalid_argument);
  std::vector<glm::vec4> values(plane.sampleCount(), {0, 0, 0, 1});
  REQUIRE_THROWS_AS(df::verifyFieldCells(plane, values, {}, 3, 1), std::invalid_argument);
  REQUIRE_THROWS_AS(df::verifyFieldCells(plane, values, {}, 7), std::invalid_argument);
  const auto singleton = domain(true, 1);
  REQUIRE_FALSE(df::verifyFieldCells(singleton, std::array{glm::vec4(0, 0, 0, 1)}, {}).complete());
}
