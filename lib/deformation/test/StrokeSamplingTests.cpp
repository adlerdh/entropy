#include "deformation/StrokeSampling.h"

#include "deformation/ContractError.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/geometric.hpp>

#include <array>
#include <vector>

using namespace deformation;

TEST_CASE("Stroke sampling carries distance and time across input event boundaries", "[deformation][stroke]")
{
  StrokeSamplingOptions options;
  options.maxEventGapSeconds = 2.0;
  options.maxDistanceMm = 0.7;
  options.maxActiveSeconds = 0.13;
  const std::array<StrokeSample, 2> sparse{{{{0.0, 0.0, 0.0}, 0.0}, {{10.0, 0.0, 0.0}, 1.0}}};
  std::vector<StrokeSample> dense;
  for (int i = 0; i <= 100; ++i)
    dense.push_back({{0.1 * i, 0.0, 0.0}, 0.01 * i});
  const auto a = resampleStroke(sparse, options);
  const auto b = resampleStroke(dense, options);
  REQUIRE(a.size() == b.size());
  double duration = 0.0;
  double displacement = 0.0;
  for (std::size_t i = 0; i < a.size(); ++i) {
    CHECK(glm::length(a[i].beginMm - b[i].beginMm) < 1.0e-10);
    CHECK(glm::length(a[i].endMm - b[i].endMm) < 1.0e-10);
    CHECK(a[i].activeSeconds == Catch::Approx(b[i].activeSeconds).margin(1.0e-12));
    CHECK(a[i].activeSeconds <= options.maxActiveSeconds + 1.0e-12);
    CHECK(glm::length(a[i].endMm - a[i].beginMm) <= options.maxDistanceMm + 1.0e-12);
    duration += a[i].activeSeconds;
    displacement += a[i].endMm.x - a[i].beginMm.x;
  }
  CHECK(duration == Catch::Approx(1.0));
  CHECK(displacement == Catch::Approx(10.0));
}

TEST_CASE("Stationary holds carry exposure but no push displacement", "[deformation][stroke]")
{
  const std::array<StrokeSample, 2> samples{{{{2.0, 3.0, 4.0}, 0.0}, {{2.0, 3.0, 4.0}, 0.2}}};
  double duration = 0.0;
  for (const auto& segment : resampleStroke(samples)) {
    CHECK(segment.beginMm == segment.endMm);
    duration += segment.activeSeconds;
  }
  CHECK(duration == Catch::Approx(0.2));
  CHECK(resampleStroke(samples, {}, true).empty());
}

TEST_CASE("Pauses and event gaps cannot create connecting warps", "[deformation][stroke]")
{
  StrokeSamplingOptions options;
  options.maxDistanceMm = 100.0;
  options.maxActiveSeconds = 1.0;
  const std::vector<StrokeSample> paused{
    {{0.0, 0.0, 0.0}, 0.0},
    {{1.0, 0.0, 0.0}, 0.1},
    {{5.0, 0.0, 0.0}, 0.2, false},
    {{50.0, 0.0, 0.0}, 5.0},
    {{51.0, 0.0, 0.0}, 5.1}};
  const auto segments = resampleStroke(paused, options);
  REQUIRE(segments.size() == 2);
  CHECK(segments[0].endMm.x == 1.0);
  CHECK(segments[1].beginMm.x == 50.0);
  CHECK(segments[0].activeSeconds + segments[1].activeSeconds == Catch::Approx(0.2));
  const std::vector<StrokeSample> gap{{{0.0, 0.0, 0.0}, 0.0}, {{10.0, 0.0, 0.0}, 10.0}};
  CHECK(resampleStroke(gap, options).empty());
}

TEST_CASE("A reversal preserves its turning point even below the nominal spacing", "[deformation][stroke]")
{
  StrokeSamplingOptions options;
  options.maxDistanceMm = 10.0;
  options.maxActiveSeconds = 1.0;
  const std::vector<StrokeSample> points{{{0.0, 0.0, 0.0}, 0.0}, {{0.2, 0.0, 0.0}, 0.1}, {{0.0, 0.0, 0.0}, 0.2}};
  const auto result = resampleStroke(points, options);
  REQUIRE(result.size() == 2);
  CHECK(result[0].endMm.x == Catch::Approx(0.2));
  CHECK(result[1].beginMm.x == Catch::Approx(0.2));
  CHECK(result[1].endMm.x == Catch::Approx(0.0));
}

TEST_CASE("Invalid stroke input fails instead of truncating or reordering", "[deformation][stroke]")
{
  std::vector<StrokeSample> points{{{0.0, 0.0, 0.0}, 0.0}, {{10.0, 0.0, 0.0}, 0.1}};
  StrokeSamplingOptions options;
  options.maxSegments = 1;
  CHECK_THROWS_AS(resampleStroke(points, options), ContractError);
  points[1].timeSeconds = 0.0;
  CHECK_THROWS_AS(resampleStroke(points), ContractError);
  points[1].timeSeconds = -1.0;
  CHECK_THROWS_AS(resampleStroke(points), ContractError);
  options.maxDistanceMm = 0.0;
  CHECK_THROWS_AS(resampleStroke({}, options), ContractError);
}
