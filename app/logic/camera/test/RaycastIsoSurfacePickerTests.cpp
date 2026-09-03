#include "logic/camera/RaycastIsoSurfacePicker.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <limits>
#include <vector>

namespace
{

std::optional<double> sampleXGradient(const glm::vec3& pixelPos)
{
  if (
    pixelPos.x < -0.5f || pixelPos.x > 9.5f || pixelPos.y < -0.5f || pixelPos.y > 0.5f || pixelPos.z < -0.5f ||
    pixelPos.z > 0.5f)
  {
    return std::nullopt;
  }
  return static_cast<double>(std::clamp(pixelPos.x, 0.0f, 9.0f));
}

camera3d::IsoSurfacePickRequest xGradientRequest(
  const glm::vec3& origin,
  const glm::vec3& direction,
  std::span<const double> isos,
  bool frontFaces = true,
  bool backFaces = true)
{
  return {
    .worldRayOrigin = origin,
    .worldRayDirection = direction,
    .pixel_T_world = glm::mat4{1.0f},
    .world_T_pixel = glm::mat4{1.0f},
    .pixelDimensions = glm::vec3{10.0f, 1.0f, 1.0f},
    .stepLength = 0.25f,
    .renderFrontFaces = frontFaces,
    .renderBackFaces = backFaces,
    .isoValues = isos,
    .sampleValue = sampleXGradient};
}

} // namespace

TEST_CASE("raycast isosurface picker returns the first front-face crossing", "[camera][raycast][picking]")
{
  const std::array<double, 1> isos{4.5};
  const auto hit =
    camera3d::pickFirstIsoSurfaceHit(xGradientRequest(glm::vec3{-2.0f, 0.0f, 0.0f}, glm::vec3{1.0f, 0.0f, 0.0f}, isos));

  REQUIRE(hit);
  CHECK(hit->isoIndex == 0);
  CHECK(hit->worldPosition.x == Catch::Approx(4.5f).margin(0.001f));
  CHECK(hit->worldPosition.y == Catch::Approx(0.0f));
  CHECK(hit->worldPosition.z == Catch::Approx(0.0f));
}

TEST_CASE("raycast isosurface picker honors front and back face flags", "[camera][raycast][picking]")
{
  const std::array<double, 1> isos{4.5};

  CHECK_FALSE(camera3d::pickFirstIsoSurfaceHit(
    xGradientRequest(glm::vec3{-2.0f, 0.0f, 0.0f}, glm::vec3{1.0f, 0.0f, 0.0f}, isos, false, true)));

  const auto backHit = camera3d::pickFirstIsoSurfaceHit(
    xGradientRequest(glm::vec3{12.0f, 0.0f, 0.0f}, glm::vec3{-1.0f, 0.0f, 0.0f}, isos, false, true));
  REQUIRE(backHit);
  CHECK(backHit->worldPosition.x == Catch::Approx(4.5f).margin(0.001f));
}

TEST_CASE("raycast isosurface picker handles ray origins inside the image", "[camera][raycast][picking]")
{
  const std::array<double, 1> isos{6.0};
  const auto hit =
    camera3d::pickFirstIsoSurfaceHit(xGradientRequest(glm::vec3{2.0f, 0.0f, 0.0f}, glm::vec3{1.0f, 0.0f, 0.0f}, isos));

  REQUIRE(hit);
  CHECK(hit->worldPosition.x == Catch::Approx(6.0f).margin(0.001f));
}

TEST_CASE(
  "raycast isosurface picker samples the ray endpoint for nonintegral step counts",
  "[camera][raycast][picking]")
{
  const std::array<double, 1> isos{8.9};
  auto request = xGradientRequest(glm::vec3{-2.0f, 0.0f, 0.0f}, glm::vec3{1.0f, 0.0f, 0.0f}, isos);
  request.stepLength = 3.0f;

  const auto hit = camera3d::pickFirstIsoSurfaceHit(request);

  REQUIRE(hit);
  CHECK(hit->worldPosition.x == Catch::Approx(8.9f).margin(0.001f));
}

TEST_CASE("raycast isosurface picker returns no hit on ray or isovalue miss", "[camera][raycast][picking]")
{
  const std::array<double, 1> isos{12.0};
  CHECK_FALSE(camera3d::pickFirstIsoSurfaceHit(
    xGradientRequest(glm::vec3{-2.0f, 0.0f, 0.0f}, glm::vec3{1.0f, 0.0f, 0.0f}, isos)));

  const std::array<double, 1> visibleIso{4.5};
  CHECK_FALSE(camera3d::pickFirstIsoSurfaceHit(
    xGradientRequest(glm::vec3{-2.0f, 2.0f, 0.0f}, glm::vec3{1.0f, 0.0f, 0.0f}, visibleIso)));
}

TEST_CASE("raycast isosurface picker rejects invalid numerical input", "[camera][raycast][picking]")
{
  const std::array<double, 1> isos{4.5};
  const float nan = std::numeric_limits<float>::quiet_NaN();

  auto request = xGradientRequest(glm::vec3{-2.0f, 0.0f, 0.0f}, glm::vec3{1.0f, 0.0f, 0.0f}, isos);
  request.pixel_T_world[0][0] = nan;
  CHECK_FALSE(camera3d::pickFirstIsoSurfaceHit(request));

  request = xGradientRequest(glm::vec3{-2.0f, 0.0f, 0.0f}, glm::vec3{1.0f, 0.0f, 0.0f}, isos);
  request.pixelDimensions.x = nan;
  CHECK_FALSE(camera3d::pickFirstIsoSurfaceHit(request));

  const std::array<double, 1> invalidIsos{std::numeric_limits<double>::quiet_NaN()};
  CHECK_FALSE(camera3d::pickFirstIsoSurfaceHit(
    xGradientRequest(glm::vec3{-2.0f, 0.0f, 0.0f}, glm::vec3{1.0f, 0.0f, 0.0f}, invalidIsos)));
}

TEST_CASE("raycast isosurface picker rejects non-finite samples", "[camera][raycast][picking]")
{
  const std::array<double, 1> isos{4.5};
  auto request = xGradientRequest(glm::vec3{-2.0f, 0.0f, 0.0f}, glm::vec3{1.0f, 0.0f, 0.0f}, isos);
  request.sampleValue = [](const glm::vec3&) {
    return std::optional<double>{std::numeric_limits<double>::infinity()};
  };

  CHECK_FALSE(camera3d::pickFirstIsoSurfaceHit(request));
}

TEST_CASE("raycast isosurface picker skips work when neither face is rendered", "[camera][raycast][picking]")
{
  const std::array<double, 1> isos{4.5};
  const auto request = xGradientRequest(glm::vec3{-2.0f, 0.0f, 0.0f}, glm::vec3{1.0f, 0.0f, 0.0f}, isos, false, false);

  CHECK_FALSE(camera3d::pickFirstIsoSurfaceHit(request));
}
