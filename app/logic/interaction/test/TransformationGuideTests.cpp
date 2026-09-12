#include "logic/interaction/TransformationGuide.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include <array>
#include <chrono>
#include <cmath>
#include <limits>

namespace
{
using Clock = interaction::TransformationGuideState::Clock;

void checkVec3(const glm::vec3& actual, const glm::vec3& expected)
{
  CHECK(actual.x == Catch::Approx(expected.x).margin(1.0e-5f));
  CHECK(actual.y == Catch::Approx(expected.y).margin(1.0e-5f));
  CHECK(actual.z == Catch::Approx(expected.z).margin(1.0e-5f));
}
} // namespace

TEST_CASE("translation guides accumulate exact applied world deltas", "[interaction][transformation_guide]")
{
  interaction::TransformationGuideState state;
  const Clock::time_point startTime{};

  CHECK_FALSE(state.guide(startTime));
  state.beginTranslation(uuids::uuid{}, {10.0f, 20.0f, 30.0f});
  CHECK(state.sourceViewUid().has_value());
  state.appendTranslation({1.0f, -2.0f, 3.5f});
  state.appendTranslation({0.5f, 4.0f, -1.0f});

  const auto guide = state.guide(startTime + std::chrono::milliseconds{20});
  REQUIRE(guide);
  const auto* translation = std::get_if<interaction::TranslationGuide>(&*guide);
  REQUIRE(translation);
  CHECK(translation->presentation.dragging);
  CHECK(translation->presentation.opacity == Catch::Approx(1.0f));
  checkVec3(translation->startWorld, {10.0f, 20.0f, 30.0f});
  checkVec3(translation->displacementWorld, {1.5f, 2.0f, 2.5f});

  const auto componentEnds = interaction::translationComponentEndpoints(*translation);
  checkVec3(componentEnds[0], {11.5f, 20.0f, 30.0f});
  checkVec3(componentEnds[1], {10.0f, 22.0f, 30.0f});
  checkVec3(componentEnds[2], {10.0f, 20.0f, 32.5f});
}

TEST_CASE("completed transformation guides fade then expire", "[interaction][transformation_guide]")
{
  interaction::TransformationGuideState state;
  const Clock::time_point startTime{};
  state.beginTranslation(uuids::uuid{}, {0.0f, 0.0f, 0.0f});
  state.appendTranslation({1.0f, 2.0f, 3.0f});
  state.finish(startTime + std::chrono::milliseconds{100});

  const auto halfway = state.guide(startTime + std::chrono::milliseconds{350});
  REQUIRE(halfway);
  CHECK_FALSE(interaction::guideIsDragging(*halfway));
  const auto* translation = std::get_if<interaction::TranslationGuide>(&*halfway);
  REQUIRE(translation);
  CHECK(translation->presentation.opacity == Catch::Approx(0.5f));
  CHECK_FALSE(state.guide(startTime + std::chrono::milliseconds{600}));
}

TEST_CASE("invalid transformation guide input is ignored safely", "[interaction][transformation_guide]")
{
  interaction::TransformationGuideState state;
  const Clock::time_point startTime{};
  state.beginTranslation(uuids::uuid{}, {1.0f, 2.0f, 3.0f});
  state.appendTranslation({1.0f, 0.0f, 0.0f});
  state.appendTranslation({std::numeric_limits<float>::infinity(), 2.0f, 3.0f});

  const auto guide = state.guide(startTime);
  REQUIRE(guide);
  const auto* translation = std::get_if<interaction::TranslationGuide>(&*guide);
  REQUIRE(translation);
  checkVec3(translation->displacementWorld, {1.0f, 0.0f, 0.0f});

  state.clear();
  CHECK_FALSE(state.guide(startTime));
  CHECK_FALSE(state.sourceViewUid().has_value());
}

TEST_CASE("rotation guides preserve exact accumulated world rotation", "[interaction][transformation_guide]")
{
  interaction::TransformationGuideState state;
  state.beginRotation(uuids::uuid{}, {1.0f, 2.0f, 3.0f}, {3.0f, 2.0f, 3.0f});
  state.appendRotation(glm::angleAxis(glm::radians(30.0f), glm::vec3{0.0f, 0.0f, 1.0f}));
  state.appendRotation(glm::angleAxis(glm::radians(15.0f), glm::vec3{0.0f, 0.0f, 1.0f}));

  const auto guide = state.guide();
  REQUIRE(guide);
  const auto* rotation = std::get_if<interaction::RotationGuide>(&*guide);
  REQUIRE(rotation);
  const interaction::RotationAxisAngle axisAngle = interaction::rotationAxisAngle(*rotation);
  checkVec3(axisAngle.axisWorld, {0.0f, 0.0f, 1.0f});
  CHECK(glm::degrees(axisAngle.angleRadians) == Catch::Approx(45.0f));

  const auto arc = interaction::rotationArcWorldPoints(*rotation, 4);
  REQUIRE(arc.size() == 5);
  checkVec3(arc.front(), {3.0f, 2.0f, 3.0f});
  checkVec3(arc.back(), {1.0f + std::sqrt(2.0f), 2.0f + std::sqrt(2.0f), 3.0f});
}

TEST_CASE("rotation guide arcs use the pointer radius within the rotation plane", "[interaction][transformation_guide]")
{
  interaction::RotationGuide guide{
    .centerWorld = {0.0f, 0.0f, 0.0f},
    .referenceWorld = {3.0f, 0.0f, 4.0f},
    .rotationWorld = glm::angleAxis(glm::radians(90.0f), glm::vec3{0.0f, 0.0f, 1.0f}),
    .presentation = {}};

  const auto arc = interaction::rotationArcWorldPoints(guide, 2);
  REQUIRE(arc.size() == 3);
  checkVec3(arc.front(), {3.0f, 0.0f, 0.0f});
  checkVec3(arc.back(), {0.0f, 3.0f, 0.0f});
  for (const glm::vec3& point : arc) {
    CHECK(glm::length(point) == Catch::Approx(3.0f));
    CHECK(point.z == Catch::Approx(0.0f));
  }
}

TEST_CASE("scale guides report factors relative to the gesture start", "[interaction][transformation_guide]")
{
  const std::array<glm::vec3, 8> initialCorners{{
    {-1.0f, -1.0f, -1.0f},
    {1.0f, -1.0f, -1.0f},
    {-1.0f, 1.0f, -1.0f},
    {1.0f, 1.0f, -1.0f},
    {-1.0f, -1.0f, 1.0f},
    {1.0f, -1.0f, 1.0f},
    {-1.0f, 1.0f, 1.0f},
    {1.0f, 1.0f, 1.0f},
  }};
  auto currentCorners = initialCorners;
  currentCorners[7] = {2.0f, 3.0f, 4.0f};

  interaction::TransformationGuideState state;
  state.beginScale(uuids::uuid{}, {0.0f, 0.0f, 0.0f}, {5.0f, 6.0f, 7.0f}, {2.0f, 4.0f, 5.0f}, initialCorners);
  state.updateScale({8.0f, 9.0f, 10.0f}, {3.0f, 2.0f, 10.0f}, currentCorners);

  const auto guide = state.guide();
  REQUIRE(guide);
  const auto* scale = std::get_if<interaction::ScaleGuide>(&*guide);
  REQUIRE(scale);
  checkVec3(interaction::relativeScaleFactors(*scale), {1.5f, 0.5f, 2.0f});
  checkVec3(scale->pointerStartWorld, {5.0f, 6.0f, 7.0f});
  checkVec3(scale->pointerCurrentWorld, {8.0f, 9.0f, 10.0f});
  checkVec3(scale->initialWorldCorners[7], {1.0f, 1.0f, 1.0f});
  checkVec3(scale->currentWorldCorners[7], {2.0f, 3.0f, 4.0f});
}

TEST_CASE("starting another transformation replaces the prior guide", "[interaction][transformation_guide]")
{
  interaction::TransformationGuideState state;
  state.beginTranslation(uuids::uuid{}, {0.0f, 0.0f, 0.0f});
  CHECK(state.isDragging<interaction::TranslationGuide>());

  state.beginRotation(uuids::uuid{}, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f});
  CHECK_FALSE(state.isDragging<interaction::TranslationGuide>());
  CHECK(state.isDragging<interaction::RotationGuide>());
}

TEST_CASE("transformation guide source view follows replacement and clear", "[interaction][transformation_guide]")
{
  interaction::TransformationGuideState state;
  const uuids::uuid translationView = uuids::uuid::from_string("f21f73ad-e6fe-42cb-8ee8-bd8ba8c729f1").value();
  const uuids::uuid scaleView = uuids::uuid::from_string("aa913249-dc53-4869-ae84-5ea893523acd").value();
  const std::array<glm::vec3, 8> corners{};

  state.beginTranslation(translationView, {0.0f, 0.0f, 0.0f});
  REQUIRE(state.sourceViewUid());
  CHECK(*state.sourceViewUid() == translationView);

  state.beginScale(scaleView, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, corners);
  REQUIRE(state.sourceViewUid());
  CHECK(*state.sourceViewUid() == scaleView);

  state.clear();
  CHECK_FALSE(state.sourceViewUid());
}

TEST_CASE("scale box outlines contain only slice-plane intersection edges", "[interaction][transformation_guide]")
{
  const std::array<glm::vec3, 8> corners{{
    {-1.0f, -1.0f, -1.0f},
    {1.0f, -1.0f, -1.0f},
    {-1.0f, 1.0f, -1.0f},
    {1.0f, 1.0f, -1.0f},
    {-1.0f, -1.0f, 1.0f},
    {1.0f, -1.0f, 1.0f},
    {-1.0f, 1.0f, 1.0f},
    {1.0f, 1.0f, 1.0f},
  }};

  const auto centerOutline = interaction::boxPlaneIntersectionOutline(corners, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f});
  REQUIRE(centerOutline.size() == 4);
  for (const glm::vec3& point : centerOutline) {
    CHECK(point.z == Catch::Approx(0.0f));
    CHECK(std::abs(point.x) == Catch::Approx(1.0f));
    CHECK(std::abs(point.y) == Catch::Approx(1.0f));
  }

  CHECK(interaction::boxPlaneIntersectionOutline(corners, {0.0f, 0.0f, 2.0f}, {0.0f, 0.0f, 1.0f}).empty());
  CHECK(interaction::boxPlaneIntersectionOutline(corners, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}).empty());

  const auto faceOutline = interaction::boxPlaneIntersectionOutline(corners, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f});
  REQUIRE(faceOutline.size() == 4);
  for (const glm::vec3& point : faceOutline) {
    CHECK(point.z == Catch::Approx(1.0f));
  }

  const auto obliqueOutline = interaction::boxPlaneIntersectionOutline(corners, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f});
  REQUIRE(obliqueOutline.size() == 6);
  for (const glm::vec3& point : obliqueOutline) {
    CHECK(point.x + point.y + point.z == Catch::Approx(0.0f).margin(1.0e-5f));
  }
}
