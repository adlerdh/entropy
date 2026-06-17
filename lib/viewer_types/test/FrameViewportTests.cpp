#include "viewer_types/FrameViewport.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/vec4.hpp>

namespace
{

void checkVec4(const glm::vec4& actual, const glm::vec4& expected)
{
  CHECK(actual.x == Catch::Approx(expected.x));
  CHECK(actual.y == Catch::Approx(expected.y));
  CHECK(actual.z == Catch::Approx(expected.z));
  CHECK(actual.w == Catch::Approx(expected.w));
}

void checkMat4(const glm::mat4& actual, const glm::mat4& expected)
{
  for (int c = 0; c < 4; ++c) {
    for (int r = 0; r < 4; ++r) {
      CHECK(actual[c][r] == Catch::Approx(expected[c][r]));
    }
  }
}

} // namespace

TEST_CASE("frame viewport defaults to the full window clip viewport", "[viewer_types][frame_viewport]")
{
  const viewer_types::FrameViewport viewport;

  CHECK(viewport.windowClipViewport() == glm::vec4{-1.0f, -1.0f, 2.0f, 2.0f});
  checkMat4(viewport.windowClip_T_viewClip(), glm::mat4{1.0f});
  checkMat4(viewport.viewClip_T_windowClip(), glm::mat4{1.0f});
}

TEST_CASE("frame viewport computes window clip transform from viewport bounds", "[viewer_types][frame_viewport]")
{
  const viewer_types::FrameViewport viewport{glm::vec4{-1.0f, 0.0f, 1.0f, 1.0f}};

  checkVec4(viewport.windowClip_T_viewClip() * glm::vec4{-1.0f, -1.0f, 0.0f, 1.0f}, glm::vec4{-1.0f, 0.0f, 0.0f, 1.0f});
  checkVec4(viewport.windowClip_T_viewClip() * glm::vec4{1.0f, 1.0f, 0.0f, 1.0f}, glm::vec4{0.0f, 1.0f, 0.0f, 1.0f});
  checkVec4(viewport.viewClip_T_windowClip() * glm::vec4{-1.0f, 0.0f, 0.0f, 1.0f}, glm::vec4{-1.0f, -1.0f, 0.0f, 1.0f});
  checkVec4(viewport.viewClip_T_windowClip() * glm::vec4{0.0f, 1.0f, 0.0f, 1.0f}, glm::vec4{1.0f, 1.0f, 0.0f, 1.0f});
}

TEST_CASE("frame viewport updates inverse transform when bounds change", "[viewer_types][frame_viewport]")
{
  viewer_types::FrameViewport viewport{glm::vec4{-1.0f, -1.0f, 1.0f, 1.0f}};

  viewport.setWindowClipViewport(glm::vec4{0.0f, -1.0f, 1.0f, 2.0f});

  CHECK(viewport.windowClipViewport() == glm::vec4{0.0f, -1.0f, 1.0f, 2.0f});
  checkVec4(viewport.windowClip_T_viewClip() * glm::vec4{-1.0f, -1.0f, 0.0f, 1.0f}, glm::vec4{0.0f, -1.0f, 0.0f, 1.0f});
  checkVec4(viewport.windowClip_T_viewClip() * glm::vec4{1.0f, 1.0f, 0.0f, 1.0f}, glm::vec4{1.0f, 1.0f, 0.0f, 1.0f});
  checkMat4(viewport.viewClip_T_windowClip() * viewport.windowClip_T_viewClip(), glm::mat4{1.0f});
}
