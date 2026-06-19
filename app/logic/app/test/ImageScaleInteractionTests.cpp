#include "logic/app/ImageScaleInteraction.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <glm/ext/matrix_transform.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec4.hpp>

namespace
{

glm::vec3 transformPoint(const glm::mat4& transform, const glm::vec3& point)
{
  const glm::vec4 transformed = transform * glm::vec4{point, 1.0f};
  return glm::vec3{transformed / transformed.w};
}

void checkVec3(const glm::vec3& actual, const glm::vec3& expected)
{
  CHECK(actual.x == Catch::Approx(expected.x));
  CHECK(actual.y == Catch::Approx(expected.y));
  CHECK(actual.z == Catch::Approx(expected.z));
}

} // namespace

TEST_CASE("image scale update keeps the scale center fixed", "[app][image-scale]")
{
  const glm::vec3 center{5.0f, 5.0f, 0.0f};
  const glm::vec3 previousPointer{6.0f, 7.0f, 0.0f};
  const glm::vec3 currentPointer{7.0f, 11.0f, 0.0f};

  const auto update = entropy::app::computeImageScaleUpdate(
    glm::mat4{1.0f},
    glm::vec3{1.0f},
    center,
    previousPointer,
    currentPointer,
    glm::vec3{1.0f, 0.0f, 0.0f},
    glm::vec3{0.0f, 1.0f, 0.0f},
    glm::vec3{0.0f, 0.0f, 1.0f},
    false);

  REQUIRE(update);

  const glm::mat4 scaledTransform =
    glm::translate(glm::mat4{1.0f}, update->m_translation) * glm::scale(glm::mat4{1.0f}, update->m_scale);
  checkVec3(transformPoint(scaledTransform, center), center);
  checkVec3(transformPoint(scaledTransform, previousPointer), currentPointer);
  checkVec3(update->m_scale, glm::vec3{2.0f, 3.0f, 1.0f});
}

TEST_CASE("image scale update can constrain scaling isotropically", "[app][image-scale]")
{
  const auto update = entropy::app::computeImageScaleUpdate(
    glm::mat4{1.0f},
    glm::vec3{1.0f},
    glm::vec3{0.0f},
    glm::vec3{2.0f, 0.0f, 0.0f},
    glm::vec3{4.0f, 0.0f, 0.0f},
    glm::vec3{1.0f, 0.0f, 0.0f},
    glm::vec3{0.0f, 1.0f, 0.0f},
    glm::vec3{0.0f, 0.0f, 1.0f},
    true);

  REQUIRE(update);
  checkVec3(update->m_scale, glm::vec3{2.0f});
  checkVec3(update->m_translation, glm::vec3{0.0f});
}

TEST_CASE("image scale update follows the pointer for an already transformed image", "[app][image-scale]")
{
  const glm::vec3 initialScale{2.0f, 3.0f, 1.0f};
  const glm::mat4 transform = glm::translate(glm::mat4{1.0f}, glm::vec3{10.0f, -4.0f, 2.0f}) *
                              glm::rotate(glm::mat4{1.0f}, glm::radians(30.0f), glm::vec3{0.0f, 0.0f, 1.0f}) *
                              glm::scale(glm::mat4{1.0f}, initialScale);

  const glm::vec3 affineCenter{1.0f, 2.0f, 0.0f};
  const glm::vec3 affinePointer{3.0f, 5.0f, 0.0f};
  const glm::vec3 expectedScaleDelta{1.5f, 0.5f, 1.0f};
  const glm::vec3 center = transformPoint(transform, affineCenter);
  const glm::vec3 previousPointer = transformPoint(transform, affinePointer);
  const glm::mat4 expectedLinear = glm::mat4{
    transform[0] * expectedScaleDelta.x,
    transform[1] * expectedScaleDelta.y,
    transform[2] * expectedScaleDelta.z,
    glm::vec4{0.0f, 0.0f, 0.0f, 1.0f}};
  const glm::vec3 currentPointer = center + glm::vec3{expectedLinear * glm::vec4{affinePointer - affineCenter, 0.0f}};

  const auto update = entropy::app::computeImageScaleUpdate(
    transform,
    initialScale,
    center,
    previousPointer,
    currentPointer,
    glm::vec3{1.0f, 0.0f, 0.0f},
    glm::vec3{0.0f, 1.0f, 0.0f},
    glm::vec3{0.0f, 0.0f, 1.0f},
    false);

  REQUIRE(update);
  checkVec3(update->m_scale, initialScale * expectedScaleDelta);

  const glm::mat4 updatedTransform = glm::translate(glm::mat4{1.0f}, update->m_translation) *
                                     glm::rotate(glm::mat4{1.0f}, glm::radians(30.0f), glm::vec3{0.0f, 0.0f, 1.0f}) *
                                     glm::scale(glm::mat4{1.0f}, update->m_scale);
  checkVec3(transformPoint(updatedTransform, affineCenter), center);
  checkVec3(transformPoint(updatedTransform, affinePointer), currentPointer);
}

TEST_CASE("image scale update rejects drags that cannot define a stable scale", "[app][image-scale]")
{
  const auto update = entropy::app::computeImageScaleUpdate(
    glm::mat4{1.0f},
    glm::vec3{1.0f},
    glm::vec3{0.0f},
    glm::vec3{0.0f},
    glm::vec3{1.0f, 1.0f, 0.0f},
    glm::vec3{1.0f, 0.0f, 0.0f},
    glm::vec3{0.0f, 1.0f, 0.0f},
    glm::vec3{0.0f, 0.0f, 1.0f},
    true);

  CHECK_FALSE(update);
}
