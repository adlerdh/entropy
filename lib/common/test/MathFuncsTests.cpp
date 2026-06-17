#include "common/MathFuncs.h"
#include "common/Geometry.h"
#include "common/IntersectionTypes.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <map>
#include <vector>

TEST_CASE("subject image dimensions are pixel dimensions scaled by spacing", "[common][math]")
{
  const glm::dvec3 dims = math::computeSubjectImageDimensions({4, 5, 6}, {0.5, 2.0, 3.0});

  CHECK(dims.x == Catch::Approx(2.0));
  CHECK(dims.y == Catch::Approx(10.0));
  CHECK(dims.z == Catch::Approx(18.0));
}

TEST_CASE("pixel-to-texture transform maps voxel centers into normalized texture coordinates", "[common][math]")
{
  const glm::dmat4 pixel_T_texture = math::computeImagePixelToTextureTransformation({4, 5, 2});

  const glm::dvec4 first = pixel_T_texture * glm::dvec4{0.0, 0.0, 0.0, 1.0};
  const glm::dvec4 last = pixel_T_texture * glm::dvec4{3.0, 4.0, 1.0, 1.0};

  CHECK(first.x == Catch::Approx(0.125));
  CHECK(first.y == Catch::Approx(0.1));
  CHECK(first.z == Catch::Approx(0.25));

  CHECK(last.x == Catch::Approx(0.875));
  CHECK(last.y == Catch::Approx(0.9));
  CHECK(last.z == Catch::Approx(0.75));
}

TEST_CASE("HSV samples are deterministic for a fixed seed and stay inside requested ranges", "[common][math]")
{
  const auto first = math::generateRandomHsvSamples(4, {0.1f, 0.2f}, {0.3f, 0.6f}, {0.7f, 0.9f}, 1234);
  const auto second = math::generateRandomHsvSamples(4, {0.1f, 0.2f}, {0.3f, 0.6f}, {0.7f, 0.9f}, 1234);

  REQUIRE(first.size() == 4);
  REQUIRE(second.size() == first.size());

  for (size_t i = 0; i < first.size(); ++i) {
    CHECK(first[i].x == Catch::Approx(second[i].x));
    CHECK(first[i].y == Catch::Approx(second[i].y));
    CHECK(first[i].z == Catch::Approx(second[i].z));

    CHECK(first[i].x >= 0.1f);
    CHECK(first[i].x <= 0.2f);
    CHECK(first[i].y >= 0.3f);
    CHECK(first[i].y <= 0.6f);
    CHECK(first[i].z >= 0.7f);
    CHECK(first[i].z <= 0.9f);
  }
}

TEST_CASE("pixel-to-subject transform combines directions spacing and origin", "[common][math]")
{
  const glm::dmat4 subject_T_pixel =
    math::computeImagePixelToSubjectTransformation(glm::dmat3{1.0}, {2.0, 3.0, 4.0}, {10.0, 20.0, 30.0});

  const glm::dvec4 subject = subject_T_pixel * glm::dvec4{1.0, 2.0, 3.0, 1.0};

  CHECK(subject.x == Catch::Approx(12.0));
  CHECK(subject.y == Catch::Approx(26.0));
  CHECK(subject.z == Catch::Approx(42.0));
  CHECK(subject.w == Catch::Approx(1.0));
}

TEST_CASE("inverse pixel dimensions are reciprocal dimensions", "[common][math]")
{
  const glm::vec3 invDims = math::computeInvPixelDimensions({2, 4, 8});

  CHECK(invDims.x == Catch::Approx(0.5f));
  CHECK(invDims.y == Catch::Approx(0.25f));
  CHECK(invDims.z == Catch::Approx(0.125f));
}

TEST_CASE("pixel AABB corners cover voxel edge coordinates", "[common][math]")
{
  const auto corners = math::computeImagePixelAABBoxCorners({2, 3, 4});

  REQUIRE(corners.size() == 8);
  CHECK(corners.front() == glm::vec3{-0.5f, -0.5f, -0.5f});
  CHECK(corners.back() == glm::vec3{1.5f, 2.5f, 3.5f});
}

TEST_CASE("subject bounding box helpers compute transformed corners and min/max", "[common][math]")
{
  const auto subjectCorners =
    math::computeImageSubjectBoundingBoxCorners({2, 2, 1}, glm::mat3{1.0f}, {2.0f, 3.0f, 4.0f}, {10.0f, 20.0f, 30.0f});
  const auto [minCorner, maxCorner] = math::computeMinMaxCornersOfAABBox(subjectCorners);

  CHECK(minCorner.x == Catch::Approx(9.0f));
  CHECK(minCorner.y == Catch::Approx(18.5f));
  CHECK(minCorner.z == Catch::Approx(28.0f));
  CHECK(maxCorner.x == Catch::Approx(13.0f));
  CHECK(maxCorner.y == Catch::Approx(24.5f));
  CHECK(maxCorner.z == Catch::Approx(32.0f));

  const auto aabbCorners = math::computeAllAABBoxCornersFromMinMaxCorners({minCorner, maxCorner});
  REQUIRE(aabbCorners.size() == 8);
  CHECK(aabbCorners.front() == minCorner);
  CHECK(aabbCorners.back() == maxCorner);
}

TEST_CASE("AABB plane intersection detects intersecting and separated planes", "[common][math]")
{
  CHECK(math::testAABBoxPlaneIntersection(glm::vec3{0.0f}, glm::vec3{1.0f}, glm::vec4{1.0f, 0.0f, 0.0f, 0.0f}));
  CHECK_FALSE(math::testAABBoxPlaneIntersection(glm::vec3{0.0f}, glm::vec3{1.0f}, glm::vec4{1.0f, 0.0f, 0.0f, -2.0f}));
}

TEST_CASE("common geometry creates planes and intersects vectors", "[common][geometry]")
{
  const glm::vec4 plane = math::makePlane(glm::vec3{0.0f, 0.0f, 1.0f}, glm::vec3{1.0f, 2.0f, 3.0f});
  CHECK(plane == glm::vec4{0.0f, 0.0f, 1.0f, -3.0f});

  float distance = 0.0f;
  REQUIRE(math::vectorPlaneIntersection(glm::vec3{0.0f, 0.0f, 10.0f}, glm::vec3{0.0f, 0.0f, -1.0f}, plane, distance));
  CHECK(distance == Catch::Approx(7.0f));

  REQUIRE(
    math::lineSegmentPlaneIntersection(glm::vec3{0.0f, 0.0f, 4.0f}, glm::vec3{0.0f, 0.0f, 2.0f}, plane, distance));
  CHECK(distance == Catch::Approx(0.5f));
}

TEST_CASE("common geometry computes AABB plane intersection polygons", "[common][geometry]")
{
  const auto corners = math::makeAABBoxCorners<float>({glm::vec3{-1.0f}, glm::vec3{1.0f}});
  const glm::vec4 plane = math::makePlane(glm::vec3{0.0f, 0.0f, 1.0f}, glm::vec3{0.0f});

  const auto intersections = math::computeAABBoxPlaneIntersections<float>(corners, plane);

  REQUIRE(intersections);
  REQUIRE(intersections->size() == intersection::k_numIntersectionVertices);
  for (const auto& point : *intersections) {
    CHECK(point.z == Catch::Approx(0.0f));
    CHECK(point.x >= -1.0f);
    CHECK(point.x <= 1.0f);
    CHECK(point.y >= -1.0f);
    CHECK(point.y <= 1.0f);
  }

  CHECK_FALSE(math::computeAABBoxPlaneIntersections<float>(
    corners,
    math::makePlane(glm::vec3{0.0f, 0.0f, 1.0f}, glm::vec3{0.0f, 0.0f, 3.0f})));
}

TEST_CASE("direction matrix helpers produce anatomical orientation codes", "[common][math]")
{
  const auto [identityCode, identityOblique] = math::computeSpiralCodeFromDirectionMatrix(glm::dmat3{1.0});
  CHECK(identityCode == "LPS");
  CHECK_FALSE(identityOblique);

  const glm::dmat3 oblique{
    glm::normalize(glm::dvec3{1.0, 1.0, 0.0}),
    glm::dvec3{0.0, 1.0, 0.0},
    glm::dvec3{0.0, 0.0, -1.0}};
  const auto [obliqueCode, obliqueFlag] = math::computeSpiralCodeFromDirectionMatrix(oblique);
  CHECK(obliqueCode == "LPI");
  CHECK(obliqueFlag);

  const glm::dmat3 closest = math::computeClosestOrthogonalDirectionMatrix(oblique);
  CHECK(closest[0] == glm::dvec3{1.0, 0.0, 0.0});
  CHECK(closest[1] == glm::dvec3{0.0, 1.0, 0.0});
  CHECK(closest[2] == glm::dvec3{0.0, 0.0, -1.0});
}

TEST_CASE("ray and box helpers return expected entry and exit distances", "[common][math]")
{
  const glm::vec3 rayStart{-1.0f, 0.5f, 0.5f};
  const glm::vec3 rayDir{1.0f, 0.0f, 0.0f};
  const glm::vec3 boxMin{0.0f, 0.0f, 0.0f};
  const glm::vec3 boxMax{1.0f, 1.0f, 1.0f};

  CHECK(math::computeRayAABBoxIntersection(rayStart, rayDir, boxMin, boxMax) == Catch::Approx(1.0f));

  const auto [hitEntry, hitExit] = math::hits(rayStart, rayDir, boxMin, boxMax);
  CHECK(hitEntry == Catch::Approx(1.0f));
  CHECK(hitExit == Catch::Approx(glm::distance(boxMin, boxMax)));

  const auto [doesHit, slabEntry, slabExit] = math::slabs(rayStart, rayDir, boxMin, boxMax);
  CHECK(doesHit);
  CHECK(slabEntry == Catch::Approx(1.0f));
  CHECK(slabExit == Catch::Approx(2.0f));

  const auto [misses, missEntry, missExit] = math::slabs(glm::vec3{-1.0f, 2.0f, 0.5f}, rayDir, boxMin, boxMax);
  CHECK_FALSE(misses);
  CHECK(missEntry > missExit);
}

TEST_CASE("ray and line segment helper handles hit miss and parallel cases", "[common][math]")
{
  const auto hit = math::computeRayLineSegmentIntersection(
    glm::vec2{0.0f, 0.0f},
    glm::vec2{1.0f, 0.0f},
    glm::vec2{2.0f, -1.0f},
    glm::vec2{2.0f, 1.0f});
  REQUIRE(hit.has_value());
  CHECK(*hit == Catch::Approx(2.0f));

  CHECK_FALSE(math::computeRayLineSegmentIntersection(
                glm::vec2{0.0f, 0.0f},
                glm::vec2{1.0f, 0.0f},
                glm::vec2{-2.0f, -1.0f},
                glm::vec2{-2.0f, 1.0f})
                .has_value());

  CHECK_FALSE(math::computeRayLineSegmentIntersection(
                glm::vec2{0.0f, 0.0f},
                glm::vec2{1.0f, 0.0f},
                glm::vec2{2.0f, 1.0f},
                glm::vec2{3.0f, 1.0f})
                .has_value());
}

TEST_CASE("ray and 2D box helper returns forward and optional reverse intersections", "[common][math]")
{
  const auto forwardHits = math::computeRayAABoxIntersections({0.5f, 0.5f}, {1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 1.0f});
  REQUIRE(forwardHits.size() == 1);
  CHECK(forwardHits[0] == glm::vec2{1.0f, 0.5f});

  const auto bothHits =
    math::computeRayAABoxIntersections({0.5f, 0.5f}, {1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 1.0f}, true);
  REQUIRE(bothHits.size() == 2);
  CHECK(bothHits[0] == glm::vec2{1.0f, 0.5f});
  CHECK(bothHits[1] == glm::vec2{0.0f, 0.5f});
}

TEST_CASE("polygon and table interpolation helpers cover boundary behavior", "[common][math]")
{
  const std::vector<glm::vec2> square{{0.0f, 0.0f}, {2.0f, 0.0f}, {2.0f, 2.0f}, {0.0f, 2.0f}};
  CHECK(math::pnpoly(square, {1.0f, 1.0f}) == 1);
  CHECK(math::pnpoly(square, {3.0f, 1.0f}) == 0);

  CHECK(math::interpolate(5.0f, {}) == Catch::Approx(0.0f));
  CHECK(math::interpolate(-1.0f, {{0.0f, 10.0f}, {10.0f, 20.0f}}) == Catch::Approx(10.0f));
  CHECK(math::interpolate(20.0f, {{0.0f, 10.0f}, {10.0f, 20.0f}}) == Catch::Approx(20.0f));
  CHECK(math::interpolate(5.0f, {{0.0f, 10.0f}, {10.0f, 20.0f}}) == Catch::Approx(15.0f));
}
