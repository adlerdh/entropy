#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace entropy::clipboard
{

struct Rgb8
{
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;

  friend bool operator==(const Rgb8&, const Rgb8&) = default;
};

struct ColoredTextRun
{
  std::string text;
  std::optional<Rgb8> color;
};

std::string rgbCssHex(Rgb8 color);
std::string escapeHtmlChar(char c);
std::string escapeHtmlText(std::string_view text);
std::string escapeRtfText(std::string_view text);
std::string rtfDocument(
  std::span<const ColoredTextRun> runs,
  int fontSizeHalfPoints = 18,
  std::optional<Rgb8> backgroundColor = std::nullopt);
std::string windowsHtmlClipboardPayload(std::string_view html);

} // namespace entropy::clipboard
