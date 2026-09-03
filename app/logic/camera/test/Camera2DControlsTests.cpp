#include "logic/camera/Camera2DControls.h"

#include "logic/camera/Camera.h"
#include "logic/camera/CameraHelpers.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/gtc/constants.hpp>
#include <glm/vector_relational.hpp>

#include <cmath>
#include <limits>

namespace
{

void checkVec2(const glm::vec2& actual, const glm::vec2& expected)
{
  CHECK(actual.x == Catch::Approx(expected.x).margin(1.0e-5f));
  CHECK(actual.y == Catch::Approx(expected.y).margin(1.0e-5f));
}

void checkVec3(const glm::vec3& actual, const glm::vec3& expected)
{
  CHECK(actual.x == Catch::Approx(expected.x).margin(1.0e-5f));
  CHECK(actual.y == Catch::Approx(expected.y).margin(1.0e-5f));
  CHECK(actual.z == Catch::Approx(expected.z).margin(1.0e-5f));
}

} // namespace

TEST_CASE("2D zoom factors are positive and reciprocal", "[camera][2d][interaction]")
{
  CHECK(camera2d::dragZoomFactor(0.4f) * camera2d::dragZoomFactor(-0.4f) == Catch::Approx(1.0f));
  CHECK(camera2d::scrollZoomFactor(12.0f) * camera2d::scrollZoomFactor(-12.0f) == Catch::Approx(1.0f));
  CHECK(camera2d::dragZoomFactor(std::numeric_limits<float>::quiet_NaN()) == Catch::Approx(1.0f));
}

TEST_CASE("2D interaction pivots distinguish crosshairs from view center", "[camera][2d][interaction]")
{
  Camera camera(ProjectionType::Orthographic);
  camera.setDefaultFov(glm::vec2{20.0f});
  const glm::vec3 crosshairs{5.0f, -2.0f, -10.0f};

  const auto crosshairsPivot = camera2d::interactionPivotNdc(camera, crosshairs);
  const auto viewCenterPivot = camera2d::interactionPivotNdc(camera, std::nullopt);

  REQUIRE(crosshairsPivot);
  REQUIRE(viewCenterPivot);
  checkVec2(*crosshairsPivot, glm::vec2{0.5f, -0.2f});
  checkVec2(*viewCenterPivot, glm::vec2{0.0f});
}

TEST_CASE("2D synchronized pan applies the source camera's exact World translation", "[camera][2d][interaction]")
{
  Camera source(ProjectionType::Orthographic);
  Camera synchronized(ProjectionType::Orthographic);
  source.setDefaultFov(glm::vec2{20.0f});
  synchronized.setDefaultFov(glm::vec2{40.0f});
  const glm::vec3 synchronizedOrigin = helper::worldOrigin(synchronized);

  camera2d::Controller sourceController{source};
  camera2d::Controller synchronizedController{synchronized};
  const auto translation = sourceController.pan(glm::vec2{0.0f}, glm::vec2{0.2f, -0.1f}, glm::vec3{0.0f});

  REQUIRE(translation);
  checkVec2(glm::vec2{helper::ndc_T_world(source, glm::vec3{0.0f})}, glm::vec2{0.2f, -0.1f});
  REQUIRE(synchronizedController.translateWorld(*translation));
  checkVec3(helper::worldOrigin(synchronized) - synchronizedOrigin, *translation);
}

TEST_CASE("2D zoom keeps its World-space pivot fixed on screen", "[camera][2d][interaction]")
{
  Camera camera(ProjectionType::Orthographic);
  camera.setDefaultFov(glm::vec2{20.0f});
  const glm::vec3 worldPivot{4.0f, -3.0f, -10.0f};
  const glm::vec2 ndcPivot{helper::ndc_T_world(camera, worldPivot)};
  const glm::vec3 initialOrigin = helper::worldOrigin(camera);
  camera2d::Controller controller{camera};

  REQUIRE(controller.zoom(2.0f, ndcPivot));
  checkVec2(glm::vec2{helper::ndc_T_world(camera, worldPivot)}, ndcPivot);
  REQUIRE(controller.zoom(0.5f, ndcPivot));

  CHECK(camera.getZoom() == Catch::Approx(1.0f));
  checkVec3(helper::worldOrigin(camera), initialOrigin);
}

TEST_CASE("2D synchronized rotation preserves each view's own pivot", "[camera][2d][interaction]")
{
  Camera source(ProjectionType::Orthographic);
  Camera synchronized(ProjectionType::Orthographic);
  source.setDefaultFov(glm::vec2{20.0f});
  synchronized.setDefaultFov(glm::vec2{20.0f});
  helper::setCameraOrigin(synchronized, glm::vec3{3.0f, 0.0f, 0.0f});
  const glm::vec3 worldPivot{5.0f, 2.0f, -10.0f};
  const auto sourcePivot = camera2d::interactionPivotNdc(source, worldPivot);
  const auto synchronizedPivot = camera2d::interactionPivotNdc(synchronized, worldPivot);
  REQUIRE(sourcePivot);
  REQUIRE(synchronizedPivot);
  REQUIRE(glm::any(glm::notEqual(*sourcePivot, *synchronizedPivot)));

  camera2d::Controller sourceController{source};
  camera2d::Controller synchronizedController{synchronized};
  const auto angle = sourceController.rotateFromPointer(
    *sourcePivot + glm::vec2{0.4f, 0.0f},
    *sourcePivot + glm::vec2{0.0f, 0.4f},
    *sourcePivot);
  REQUIRE(angle);
  REQUIRE(synchronizedController.rotate(*angle, *synchronizedPivot));

  const auto rotatedSourcePivot = camera2d::interactionPivotNdc(source, worldPivot);
  const auto rotatedSynchronizedPivot = camera2d::interactionPivotNdc(synchronized, worldPivot);
  REQUIRE(rotatedSourcePivot);
  REQUIRE(rotatedSynchronizedPivot);
  checkVec2(*rotatedSourcePivot, *sourcePivot);
  checkVec2(*rotatedSynchronizedPivot, *synchronizedPivot);
  CHECK(*angle == Catch::Approx(glm::half_pi<float>()));
}

TEST_CASE("2D controller ignores zero and non-finite input", "[camera][2d][interaction]")
{
  Camera camera(ProjectionType::Orthographic);
  const glm::mat4 transform = camera.camera_T_anatomy();
  const float nan = std::numeric_limits<float>::quiet_NaN();
  camera2d::Controller controller{camera};

  CHECK_FALSE(controller.pan(glm::vec2{0.0f}, glm::vec2{0.0f}, glm::vec3{0.0f}));
  CHECK_FALSE(controller.rotateFromPointer(glm::vec2{nan}, glm::vec2{1.0f}, glm::vec2{0.0f}));
  CHECK_FALSE(controller.rotate(nan, glm::vec2{0.0f}));
  CHECK_FALSE(controller.zoom(nan, glm::vec2{0.0f}));
  CHECK(camera.camera_T_anatomy() == transform);
}
