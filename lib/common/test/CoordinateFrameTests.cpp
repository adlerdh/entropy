#include "common/CoordinateFrame.h"
#include "common/MathFuncs.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace
{

void checkVec3(const glm::vec3& actual, const glm::vec3& expected)
{
  CHECK(actual.x == Catch::Approx(expected.x).margin(1.0e-5f));
  CHECK(actual.y == Catch::Approx(expected.y).margin(1.0e-5f));
  CHECK(actual.z == Catch::Approx(expected.z).margin(1.0e-5f));
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
