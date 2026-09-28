#include "deformation/BrushStep.h"

#include "Fixtures.h"
#include "deformation/ContractError.h"
#include "deformation/VelocityLattice.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/geometric.hpp>

#include <cmath>
#include <initializer_list>
#include <limits>
#include <vector>

using namespace deformation;

TEST_CASE("Brush strength acts on physical velocity and compact support", "[deformation][brush]")
{
  BrushDefinition d;
  d.radiusMm = 10.0;
  d.strength = 2.0;
  d.motion = PushMotion{{1.0, 2.0, 3.0}};
  const BrushStep push{d};
  CHECK(push.velocity({0.0, 0.0, 0.0}) == glm::dvec3{2.0, 4.0, 6.0});
  CHECK(push.envelope({5.0, 0.0, 0.0}) == Catch::Approx(0.1875));
  CHECK(push.velocity({10.0, 0.0, 0.0}) == glm::dvec3{0.0});
  CHECK(push.velocity({100.0, 0.0, 0.0}) == glm::dvec3{0.0});
  d.motion = RadialMotion{0.2};
  CHECK(BrushStep{d}.generator({1.0, 0.0, 0.0}).x == Catch::Approx(0.4));
  d.motion = RadialMotion{-0.2};
  CHECK(BrushStep{d}.generator({1.0, 0.0, 0.0}).x == Catch::Approx(-0.4));
  d.motion = TwirlMotion{{0.0, 0.0, 1.0}, 0.3};
  CHECK(BrushStep{d}.generator({2.0, 0.0, 0.0}).y == Catch::Approx(1.2));
  d.strength = 0.0;
  CHECK(BrushStep{d}.velocity({2.0, 0.0, 0.0}) == glm::dvec3{0.0});
}

TEST_CASE("Protection has an exactly stationary core and smooth physical transition", "[deformation][brush]")
{
  BrushDefinition d;
  d.radiusMm = 20.0;
  d.motion = PushMotion{{1.0, 0.0, 0.0}};
  d.protection.push_back({{0.0, 0.0, 0.0}, 2.0, 2.0});
  const BrushStep protectedBrush{d};
  d.protection.clear();
  const BrushStep unprotected{d};
  CHECK(protectedBrush.velocity({0.0, 0.0, 0.0}) == glm::dvec3{0.0});
  CHECK(protectedBrush.velocity({2.0, 0.0, 0.0}) == glm::dvec3{0.0});
  CHECK(protectedBrush.envelope({3.0, 0.0, 0.0}) == Catch::Approx(0.5 * unprotected.envelope({3.0, 0.0, 0.0})));
  CHECK(protectedBrush.envelope({4.0, 0.0, 0.0}) == unprotected.envelope({4.0, 0.0, 0.0}));
  const double h = 1.0e-3;
  CHECK(protectedBrush.envelope({2.0 + h, 0.0, 0.0}) / h < 1.0e-5);
  CHECK(unprotected.envelope({20.0 - h, 0.0, 0.0}) / h < 1.0e-8);
  const VelocityLattice lattice{protectedBrush, 3.0};
  CHECK(lattice.velocity({1.0, 0.0, 0.0}) == glm::dvec3{0.0});
}

TEST_CASE("Cubic velocity coefficients reproduce each affine generator in 2D and 3D", "[deformation][lattice]")
{
  for (const auto dimension : {SpatialDimension::Plane, SpatialDimension::Volume}) {
    BrushDefinition d;
    d.dimension = dimension;
    d.centerMm = {1000.0, -500.0, 200.0};
    d.directions = deformation::test::obliqueFrame();
    d.radiusMm = 6.0;
    d.strength = 0.7;
    const std::vector<BrushMotion> motions{
      PushMotion{d.directions[0] * 0.4},
      RadialMotion{-0.12},
      TwirlMotion{d.directions[2], 0.2}};
    for (const auto& motion : motions) {
      d.motion = motion;
      const BrushStep brush{d};
      for (const double spacing : {0.7, 2.0, 10.0}) {
        const VelocityLattice lattice{brush, spacing};
        CHECK(lattice.coefficients().size() == lattice.domain().sampleCount());
        CHECK(lattice.domain().dimension() == dimension);
        for (const double x : {-5.9, -2.3, 0.0, 1.7, 5.9}) {
          const glm::dvec3 local{x, 0.3, dimension == SpatialDimension::Plane ? 0.0 : 0.5};
          const glm::dvec3 point = d.centerMm + d.directions * local;
          CHECK(glm::length(lattice.velocity(point) - brush.velocity(point)) < 1.0e-10);
          if (dimension == SpatialDimension::Plane) {
            CHECK(std::abs(glm::dot(lattice.velocity(point), d.directions[2])) < 1.0e-12);
          }
        }
        CHECK(lattice.velocity(d.centerMm + 20.0 * d.directions[0]) == glm::dvec3{0.0});
      }
    }
  }
}

TEST_CASE("Brush and lattice constructors reject unsupported recipes", "[deformation][brush]")
{
  BrushDefinition d;
  d.radiusMm = 0.0;
  CHECK_THROWS_AS(BrushStep{d}, ContractError);
  d.radiusMm = 10.0;
  d.strength = -1.0;
  CHECK_THROWS_AS(BrushStep{d}, ContractError);
  d.strength = 1.0;
  d.motion = TwirlMotion{{0.0, 0.0, 2.0}, 1.0};
  CHECK_THROWS_AS(BrushStep{d}, ContractError);
  d.dimension = SpatialDimension::Plane;
  d.motion = PushMotion{{0.0, 0.0, 1.0}};
  CHECK_THROWS_AS(BrushStep{d}, ContractError);
  d.motion = TwirlMotion{{1.0, 0.0, 0.0}, 1.0};
  CHECK_THROWS_AS(BrushStep{d}, ContractError);
  d.motion = RadialMotion{std::numeric_limits<double>::quiet_NaN()};
  CHECK_THROWS_AS(BrushStep{d}, ContractError);
  d.motion = PushMotion{};
  d.protection.push_back({{0.0, 0.0, 1.0}, 1.0, 1.0});
  CHECK_THROWS_AS(BrushStep{d}, ContractError);
  d.protection.clear();
  const BrushStep brush{d};
  CHECK_THROWS_AS(brush.velocity({0.0, 0.0, 1.0}), ContractError);
  CHECK_THROWS_AS(VelocityLattice(brush, 0.0), ContractError);
  CHECK_THROWS_AS(VelocityLattice(brush, 1.0, 4), ContractError);
  CHECK_THROWS_AS(VelocityLattice(brush, 1.0e-100), ContractError);
}
