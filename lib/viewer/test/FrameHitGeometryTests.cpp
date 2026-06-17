#include "viewer/FrameHitGeometry.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/ext/matrix_transform.hpp>

#include <array>
#include <cstdint>

namespace
{

uuids::uuid uuidFromIndex(uint8_t index)
{
  return uuids::uuid{{index, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};
}

void checkVec2(const glm::vec2& actual, const glm::vec2& expected)
{
  CHECK(actual.x == Catch::Approx(expected.x));
  CHECK(actual.y == Catch::Approx(expected.y));
}

void checkVec4(const glm::vec4& actual, const glm::vec4& expected)
{
  CHECK(actual.x == Catch::Approx(expected.x));
  CHECK(actual.y == Catch::Approx(expected.y));
  CHECK(actual.z == Catch::Approx(expected.z));
  CHECK(actual.w == Catch::Approx(expected.w));
}

} // namespace

TEST_CASE("frame hit geometry maps window positions through frame clip space", "[viewer][frame_hit]")
{
  const Viewport windowViewport{0.0f, 0.0f, 200.0f, 100.0f};
  const glm::mat4 viewClip_T_windowClip = glm::scale(glm::mat4{1.0f}, glm::vec3{2.0f, 1.0f, 1.0f});
  const glm::mat4 world_T_viewClip = glm::translate(glm::mat4{1.0f}, glm::vec3{10.0f, 20.0f, 30.0f});

  const auto hit = viewer::computeFrameHitGeometry(
    windowViewport,
    glm::vec2{150.0f, 25.0f},
    viewClip_T_windowClip,
    world_T_viewClip,
    0.25f);

  checkVec2(hit.m_windowClipPos, glm::vec2{0.5f, -0.5f});
  checkVec2(hit.m_viewClipPos, glm::vec2{1.0f, -0.5f});
  checkVec4(hit.m_worldPos, glm::vec4{11.0f, 19.5f, 30.25f, 1.0f});
}

TEST_CASE("frame hit geometry handles perspective divide", "[viewer][frame_hit]")
{
  const Viewport windowViewport{0.0f, 0.0f, 100.0f, 100.0f};
  glm::mat4 world_T_viewClip{1.0f};
  world_T_viewClip[3][3] = 2.0f;

  const auto hit =
    viewer::computeFrameHitGeometry(windowViewport, glm::vec2{100.0f, 100.0f}, glm::mat4{1.0f}, world_T_viewClip, 0.0f);

  checkVec2(hit.m_windowClipPos, glm::vec2{1.0f, 1.0f});
  checkVec2(hit.m_viewClipPos, glm::vec2{1.0f, 1.0f});
  checkVec4(hit.m_worldPos, glm::vec4{0.5f, 0.5f, 0.0f, 1.0f});
}

TEST_CASE("frame hit geometry finds the first frame containing a window position", "[viewer][frame_hit]")
{
  const Viewport windowViewport{0.0f, 0.0f, 200.0f, 100.0f};
  const std::array frames{
    viewer::FrameHitTarget{.m_uid = uuidFromIndex(1), .m_windowClipViewport = {-1.0f, -1.0f, 1.0f, 2.0f}},
    viewer::FrameHitTarget{.m_uid = uuidFromIndex(2), .m_windowClipViewport = {0.0f, -1.0f, 1.0f, 2.0f}}};

  CHECK(viewer::findFrameAtWindowPosition(windowViewport, {50.0f, 50.0f}, frames) == std::optional{uuidFromIndex(1)});
  CHECK(viewer::findFrameAtWindowPosition(windowViewport, {150.0f, 50.0f}, frames) == std::optional{uuidFromIndex(2)});
}

TEST_CASE("frame hit geometry treats right and top edges as outside", "[viewer][frame_hit]")
{
  const Viewport windowViewport{0.0f, 0.0f, 100.0f, 100.0f};
  const std::array frames{
    viewer::FrameHitTarget{.m_uid = uuidFromIndex(1), .m_windowClipViewport = {-1.0f, -1.0f, 2.0f, 2.0f}}};

  CHECK_FALSE(viewer::findFrameAtWindowPosition(windowViewport, {100.0f, 50.0f}, frames));
  CHECK_FALSE(viewer::findFrameAtWindowPosition(windowViewport, {50.0f, 100.0f}, frames));
  CHECK_FALSE(
    viewer::findFrameAtWindowPosition(windowViewport, {50.0f, 50.0f}, std::span<const viewer::FrameHitTarget>{}));
}

TEST_CASE("frame hit geometry finds the largest frame by viewport area", "[viewer][frame_hit]")
{
  const std::array frames{
    viewer::FrameHitTarget{.m_uid = uuidFromIndex(1), .m_windowClipViewport = {-1.0f, -1.0f, 1.0f, 1.0f}},
    viewer::FrameHitTarget{.m_uid = uuidFromIndex(2), .m_windowClipViewport = {0.0f, -1.0f, 1.0f, 2.0f}},
    viewer::FrameHitTarget{.m_uid = uuidFromIndex(3), .m_windowClipViewport = {-1.0f, 0.0f, 1.0f, 1.5f}}};

  CHECK(viewer::findLargestFrameByArea(frames) == std::optional{uuidFromIndex(2)});
}

TEST_CASE("frame hit geometry keeps the first frame when largest areas tie", "[viewer][frame_hit]")
{
  const std::array frames{
    viewer::FrameHitTarget{.m_uid = uuidFromIndex(1), .m_windowClipViewport = {-1.0f, -1.0f, 1.0f, 2.0f}},
    viewer::FrameHitTarget{.m_uid = uuidFromIndex(2), .m_windowClipViewport = {0.0f, -1.0f, 2.0f, 1.0f}}};

  CHECK(viewer::findLargestFrameByArea(frames) == std::optional{uuidFromIndex(1)});
  CHECK_FALSE(viewer::findLargestFrameByArea(std::span<const viewer::FrameHitTarget>{}));
}

TEST_CASE("frame hit selection uses the direct hit as the event target", "[viewer][frame_hit]")
{
  const auto hitUid = uuidFromIndex(1);
  const auto overrideUid = uuidFromIndex(2);

  const auto selection = viewer::resolveFrameHitSelection(hitUid, overrideUid);

  REQUIRE(selection);
  CHECK(selection->m_hitFrameUid == hitUid);
  CHECK(selection->m_transformFrameUid == overrideUid);
}

TEST_CASE("frame hit selection falls back to the override when there is no direct hit", "[viewer][frame_hit]")
{
  const auto overrideUid = uuidFromIndex(2);

  const auto selection = viewer::resolveFrameHitSelection(std::nullopt, overrideUid);

  REQUIRE(selection);
  CHECK(selection->m_hitFrameUid == overrideUid);
  CHECK(selection->m_transformFrameUid == overrideUid);
}

TEST_CASE("frame hit selection is empty without a direct hit or override", "[viewer][frame_hit]")
{
  CHECK_FALSE(viewer::resolveFrameHitSelection(std::nullopt, std::nullopt));
}
