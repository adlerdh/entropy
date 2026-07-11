#include "rendering/ascii/AsciiClipboard.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>

namespace entropy::rendering::ascii_clipboard
{

entropy::clipboard::Rgb8 toRgb8(const glm::vec3& rgb)
{
  const glm::vec3 clamped = glm::clamp(rgb, glm::vec3{0.0f}, glm::vec3{1.0f});
  return entropy::clipboard::Rgb8{
    static_cast<uint8_t>(std::round(clamped.r * 255.0f)),
    static_cast<uint8_t>(std::round(clamped.g * 255.0f)),
    static_cast<uint8_t>(std::round(clamped.b * 255.0f))};
}

glm::vec3 foregroundColorForCell(const glm::vec3& srcRgb, bool useColormap, const glm::vec3& fixedForeground)
{
  if (!useColormap) {
    return fixedForeground;
  }

  const float maxComponent = std::max({srcRgb.r, srcRgb.g, srcRgb.b});
  const float minComponent = std::min({srcRgb.r, srcRgb.g, srcRgb.b});
  const float chroma = maxComponent - minComponent;
  if (chroma <= 1.0e-6f) {
    return glm::vec3{1.0f};
  }

  float hue = 0.0f;
  if (maxComponent == srcRgb.r) {
    hue = std::fmod((srcRgb.g - srcRgb.b) / chroma, 6.0f);
  }
  else if (maxComponent == srcRgb.g) {
    hue = ((srcRgb.b - srcRgb.r) / chroma) + 2.0f;
  }
  else {
    hue = ((srcRgb.r - srcRgb.g) / chroma) + 4.0f;
  }
  hue /= 6.0f;
  if (hue < 0.0f) {
    hue += 1.0f;
  }

  const float saturation = chroma / std::max(maxComponent, 1.0e-6f);
  const float h6 = hue * 6.0f;
  const float c = saturation;
  const float x = c * (1.0f - std::abs(std::fmod(h6, 2.0f) - 1.0f));
  const float m = 1.0f - c;
  glm::vec3 rgbPrime{0.0f};
  if (h6 < 1.0f)
    rgbPrime = {c, x, 0.0f};
  else if (h6 < 2.0f)
    rgbPrime = {x, c, 0.0f};
  else if (h6 < 3.0f)
    rgbPrime = {0.0f, c, x};
  else if (h6 < 4.0f)
    rgbPrime = {0.0f, x, c};
  else if (h6 < 5.0f)
    rgbPrime = {x, 0.0f, c};
  else
    rgbPrime = {c, 0.0f, x};
  return rgbPrime + glm::vec3{m};
}

std::string colorSpanOpen(entropy::clipboard::Rgb8 color)
{
  return "<span style=\"color:" + entropy::clipboard::rgbCssHex(color) + "\">";
}

} // namespace entropy::rendering::ascii_clipboard
