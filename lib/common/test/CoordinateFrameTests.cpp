#include "common/CoordinateFrame.h"
#include "common/MathFuncs.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <cmath>
#include <limits>

namespace
{

void checkVec3(const glm::vec3& actual, const glm::vec3& expected)
{
  CHECK(actual.x == Catch::Approx(expected.x).margin(1.0e-5f));
  CHECK(actual.y == Catch::Approx(expected.y).margin(1.0e-5f));
  CHECK(actual.z == Catch::Approx(expected.z).margin(1.0e-5f));
}

void checkRightHandedOrthonormalRotation(const CoordinateFrame& frame, const float margin = 1.0e-5f)
{
  const glm::mat3 rotation{frame.world_T_frame()};
  for (glm::length_t axis = 0; axis < 3; ++axis) {
    CHECK(glm::length(rotation[axis]) == Catch::Approx(1.0f).margin(margin));
    for (glm::length_t other = axis + 1; other < 3; ++other) {
      CHECK(glm::dot(rotation[axis], rotation[other]) == Catch::Approx(0.0f).margin(margin));
    }
  }
  CHECK(glm::determinant(rotation) == Catch::Approx(1.0f).margin(margin));
}

} // namespace

TEST_CASE("coordinate frame defaults to the identity transform", "[common][coordinate-frame]")
{
  const CoordinateFrame frame;

  checkVec3(frame.worldOrigin(), {0.0f, 0.0f, 0.0f});
  checkVec3(glm::vec3{frame.world_T_frame() * glm::vec4{1.0f, 2.0f, 3.0f, 1.0f}}, {1.0f, 2.0f, 3.0f});
  checkVec3(glm::vec3{frame.frame_T_world() * glm::vec4{1.0f, 2.0f, 3.0f, 1.0f}}, {1.0f, 2.0f, 3.0f});
}

TEST_CASE("coordinate frame stores origin and angle-axis rotation", "[common][coordinate-frame]")
{
  CoordinateFrame frame{{1.0f, 2.0f, 3.0f}, 90.0f, {0.0f, 0.0f, 1.0f}};

  checkVec3(frame.worldOrigin(), {1.0f, 2.0f, 3.0f});
  checkVec3(glm::vec3{frame.world_T_frame() * glm::vec4{1.0f, 0.0f, 0.0f, 1.0f}}, {1.0f, 3.0f, 3.0f});

  const glm::vec4 worldPoint = frame.world_T_frame() * glm::vec4{4.0f, 5.0f, 6.0f, 1.0f};
  const glm::vec4 framePoint = frame.frame_T_world() * worldPoint;
  checkVec3(glm::vec3{framePoint}, {4.0f, 5.0f, 6.0f});
}

TEST_CASE("coordinate frame can be reset fully or rotation-only", "[common][coordinate-frame]")
{
  CoordinateFrame frame{{1.0f, 2.0f, 3.0f}, 45.0f, {0.0f, 0.0f, 1.0f}};

  frame.setIdentityRotation();
  checkVec3(frame.worldOrigin(), {1.0f, 2.0f, 3.0f});
  checkVec3(glm::vec3{frame.world_T_frame() * glm::vec4{1.0f, 1.0f, 1.0f, 1.0f}}, {2.0f, 3.0f, 4.0f});

  frame.setIdentity();
  checkVec3(frame.worldOrigin(), {0.0f, 0.0f, 0.0f});
  checkVec3(glm::vec3{frame.world_T_frame() * glm::vec4{1.0f, 1.0f, 1.0f, 1.0f}}, {1.0f, 1.0f, 1.0f});
}

TEST_CASE("coordinate frame builds rotation from paired axes", "[common][coordinate-frame]")
{
  CoordinateFrame frame{
    {3.0f, 4.0f, 5.0f},
    glm::vec3{1.0f, 0.0f, 0.0f},
    glm::vec3{0.0f, 1.0f, 0.0f},
    glm::vec3{0.0f, 1.0f, 0.0f},
    glm::vec3{-1.0f, 0.0f, 0.0f}};

  checkVec3(frame.worldOrigin(), {3.0f, 4.0f, 5.0f});
  checkVec3(glm::vec3{frame.world_T_frame() * glm::vec4{1.0f, 0.0f, 0.0f, 0.0f}}, {0.0f, 1.0f, 0.0f});
  checkVec3(glm::vec3{frame.world_T_frame() * glm::vec4{0.0f, 1.0f, 0.0f, 0.0f}}, {-1.0f, 0.0f, 0.0f});

  frame.setFrameToWorldRotation(
    glm::vec3{1.0f, 0.0f, 0.0f},
    glm::vec3{0.0f, 1.0f, 0.0f},
    glm::vec3{0.0f, 1.0f, 0.0f},
    glm::vec3{-1.0f, 0.0f, 0.0f},
    true);

  checkVec3(glm::vec3{frame.world_T_frame() * glm::vec4{1.0f, 0.0f, 0.0f, 0.0f}}, {0.0f, 1.0f, 0.0f});
  checkVec3(glm::vec3{frame.world_T_frame() * glm::vec4{0.0f, 1.0f, 0.0f, 0.0f}}, {-1.0f, 0.0f, 0.0f});

  REQUIRE_THROWS(frame.setFrameToWorldRotation(
    glm::vec3{1.0f, 0.0f, 0.0f},
    glm::vec3{0.0f, 1.0f, 0.0f},
    glm::vec3{1.0f, 0.0f, 0.0f},
    glm::vec3{0.0f, 0.0f, 1.0f},
    false));

  REQUIRE_THROWS(frame.setFrameToWorldRotation(
    glm::vec3{1.0f, 0.0f, 0.0f},
    glm::vec3{1.0f, 0.0f, 0.0f},
    glm::vec3{0.0f, 1.0f, 0.0f},
    glm::normalize(glm::vec3{1.0f, 1.0f, 0.0f}),
    true));
}

TEST_CASE("coordinate frame addition combines origins and rotations", "[common][coordinate-frame]")
{
  const CoordinateFrame lhs{{1.0f, 0.0f, 0.0f}, 90.0f, {0.0f, 0.0f, 1.0f}};
  const CoordinateFrame rhs{{0.0f, 2.0f, 0.0f}, 90.0f, {0.0f, 0.0f, 1.0f}};

  CoordinateFrame sum = lhs + rhs;
  checkVec3(sum.worldOrigin(), {1.0f, 2.0f, 0.0f});
  checkVec3(glm::vec3{sum.world_T_frame() * glm::vec4{1.0f, 0.0f, 0.0f, 0.0f}}, {-1.0f, 0.0f, 0.0f});

  sum += CoordinateFrame{{1.0f, 1.0f, 1.0f}, glm::quat{1.0f, 0.0f, 0.0f, 0.0f}};
  checkVec3(sum.worldOrigin(), {2.0f, 3.0f, 1.0f});
}

TEST_CASE("coordinate frame rotates about a world position", "[common][coordinate-frame][math]")
{
  CoordinateFrame frame{{2.0f, 0.0f, 0.0f}, glm::quat{1.0f, 0.0f, 0.0f, 0.0f}};
  const glm::quat rotation = glm::angleAxis(glm::half_pi<float>(), glm::vec3{0.0f, 0.0f, 1.0f});

  math::rotateFrameAboutWorldPos(frame, rotation, {1.0f, 0.0f, 0.0f});

  checkVec3(frame.worldOrigin(), {1.0f, 1.0f, 0.0f});
  checkVec3(glm::vec3{frame.world_T_frame() * glm::vec4{1.0f, 0.0f, 0.0f, 0.0f}}, {0.0f, 1.0f, 0.0f});
}

TEST_CASE("coordinate frame normalizes every quaternion rotation", "[common][coordinate-frame][rotation]")
{
  CoordinateFrame frame{{1.0f, 2.0f, 3.0f}, 3.0f * glm::angleAxis(0.7f, glm::normalize(glm::vec3{1.0f, 2.0f, 3.0f}))};
  checkRightHandedOrthonormalRotation(frame);
  CHECK(glm::length(frame.world_T_frame_rotation()) == Catch::Approx(1.0f));

  frame.setFrameToWorldRotation(8.0f * glm::angleAxis(-0.4f, glm::normalize(glm::vec3{-2.0f, 1.0f, 0.5f})));
  checkRightHandedOrthonormalRotation(frame);
  CHECK(glm::length(frame.world_T_frame_rotation()) == Catch::Approx(1.0f));
}

TEST_CASE("incremental crosshairs rotations remain orthonormal", "[common][coordinate-frame][rotation]")
{
  CoordinateFrame frame;
  const glm::quat increment = glm::angleAxis(glm::radians(0.1f), glm::normalize(glm::vec3{1.0f, 2.0f, 3.0f}));
  for (int step = 0; step < 10000; ++step) {
    math::rotateFrameAboutWorldPos(frame, increment, frame.worldOrigin());
  }

  checkRightHandedOrthonormalRotation(frame, 2.0e-5f);
  CHECK(glm::length(frame.world_T_frame_rotation()) == Catch::Approx(1.0f).margin(1.0e-6f));
}

TEST_CASE("coordinate frame rejects invalid rotations and axes", "[common][coordinate-frame][rotation]")
{
  CoordinateFrame frame;
  const float nan = std::numeric_limits<float>::quiet_NaN();

  CHECK_THROWS(frame.setFrameToWorldRotation(glm::quat{0.0f, 0.0f, 0.0f, 0.0f}));
  CHECK_THROWS(frame.setFrameToWorldRotation(glm::quat{1.0f, nan, 0.0f, 0.0f}));
  CHECK_THROWS(frame.setFrameToWorldRotation(10.0f, glm::vec3{0.0f}));
  CHECK_THROWS(frame.setFrameToWorldRotation(nan, glm::vec3{0.0f, 0.0f, 1.0f}));
  CHECK_THROWS(frame.setFrameToWorldRotation(
    glm::vec3{1.0f, 0.0f, 0.0f},
    glm::vec3{1.0f, 0.0f, 0.0f},
    glm::vec3{-1.0f, 0.0f, 0.0f},
    glm::vec3{0.0f, 1.0f, 0.0f},
    false));
}

TEST_CASE("frame rotation normalizes non-unit angle axes", "[common][coordinate-frame][rotation]")
{
  const CoordinateFrame frame{{0.0f, 0.0f, 0.0f}, 90.0f, glm::vec3{0.0f, 0.0f, 12.0f}};
  checkVec3(glm::vec3{frame.world_T_frame() * glm::vec4{1.0f, 0.0f, 0.0f, 0.0f}}, {0.0f, 1.0f, 0.0f});
  checkRightHandedOrthonormalRotation(frame);
}
