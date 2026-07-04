#pragma once

#include <glm/vec4.hpp>
#include <glm/vec3.hpp>

#include <optional>

namespace entropy::ui
{
struct GradientBackgroundOptions
{
  glm::vec3 edgeColor{0.10f, 0.10f, 0.10f};
  glm::vec3 centerColor{0.15f, 0.15f, 0.15f};
  float rectangularExponent = 8.0f;
  bool dither = true;
  std::optional<glm::ivec4> viewportPx;
};

/** Draws a rectangular vignette gradient into the default framebuffer. */
void renderGradientBackground(const GradientBackgroundOptions& options = {});
} // namespace entropy::ui
