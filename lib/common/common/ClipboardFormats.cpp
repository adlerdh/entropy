#include "common/ClipboardFormats.h"

#include <algorithm>
#include <cstddef>
#include <format>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <vector>

namespace clipboard
{

std::string rgbCssHex(Rgb8 color)
{
  std::ostringstream out;
  out << '#' << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << static_cast<int>(color.r)
      << std::setw(2) << static_cast<int>(color.g) << std::setw(2) << static_cast<int>(color.b);
  return out.str();
}

std::string escapeHtmlChar(char c)
{
  switch (c) {
    case '&':
      return "&amp;";
    case '<':
      return "&lt;";
    case '>':
      return "&gt;";
    case '"':
      return "&quot;";
    case '\'':
      return "&#39;";
    default:
      return std::string(1, c);
  }
}

std::string escapeHtmlText(std::string_view text)
{
  std::string escaped;
  escaped.reserve(text.size());
  for (char c : text) {
    escaped += escapeHtmlChar(c);
  }
  return escaped;
}

std::string escapeRtfText(std::string_view text)
{
  std::string escaped;
  escaped.reserve(text.size());
  for (char c : text) {
    switch (c) {
      case '\\':
        escaped += "\\\\";
        break;
      case '{':
        escaped += "\\{";
        break;
      case '}':
        escaped += "\\}";
        break;
      case '\n':
        escaped += "\\line\n";
        break;
      default:
        escaped.push_back(c);
        break;
    }
  }
  return escaped;
}

namespace
{

std::vector<Rgb8> uniqueColors(std::span<const ColoredTextRun> runs, std::optional<Rgb8> backgroundColor)
{
  std::vector<Rgb8> colors;
  if (backgroundColor) {
    colors.push_back(*backgroundColor);
  }
  for (const auto& run : runs) {
    if (!run.color) {
      continue;
    }
    if (std::find(std::begin(colors), std::end(colors), *run.color) == std::end(colors)) {
      colors.push_back(*run.color);
    }
  }
  return colors;
}

int colorTableIndex(const std::vector<Rgb8>& colors, Rgb8 color)
{
  const auto it = std::find(std::begin(colors), std::end(colors), color);
  if (it == std::end(colors)) {
    return 0;
  }
  return static_cast<int>(std::distance(std::begin(colors), it)) + 1;
}

void replaceAll(std::string& text, std::string_view needle, std::string_view replacement)
{
  std::size_t pos = 0;
  while ((pos = text.find(needle, pos)) != std::string::npos) {
    text.replace(pos, needle.size(), replacement);
    pos += replacement.size();
  }
}

} // namespace

std::string
rtfDocument(std::span<const ColoredTextRun> runs, int fontSizeHalfPoints, std::optional<Rgb8> backgroundColor)
{
  const std::vector<Rgb8> colors = uniqueColors(runs, backgroundColor);

  std::string rtf = R"({\rtf1\ansi\deff0)";
  rtf += R"({\fonttbl{\f0\fmodern Courier;}})";
  rtf += "{\\colortbl;";
  for (Rgb8 color : colors) {
    rtf += std::format(
      R"(\red{}\green{}\blue{};)",
      static_cast<int>(color.r),
      static_cast<int>(color.g),
      static_cast<int>(color.b));
  }
  rtf += "}";
  rtf += std::format("\\f0\\fs{} ", fontSizeHalfPoints);
  if (backgroundColor) {
    rtf += std::format("\\highlight{} ", colorTableIndex(colors, *backgroundColor));
  }

  int currentColorIndex = 0;
  for (const auto& run : runs) {
    const int nextColorIndex = run.color ? colorTableIndex(colors, *run.color) : 0;
    if (nextColorIndex != currentColorIndex) {
      rtf += std::format("\\cf{} ", nextColorIndex);
      currentColorIndex = nextColorIndex;
    }
    rtf += escapeRtfText(run.text);
  }

  rtf += "}";
  return rtf;
}

std::string windowsHtmlClipboardPayload(std::string_view html)
{
  constexpr std::string_view startFragmentMarker = "<!--StartFragment-->";
  constexpr std::string_view endFragmentMarker = "<!--EndFragment-->";
  const std::string markedHtml = std::string(startFragmentMarker) + std::string(html) + std::string(endFragmentMarker);

  std::string payload =
    "Version:0.9\r\n"
    "StartHTML:__________\r\n"
    "EndHTML:__________\r\n"
    "StartFragment:__________\r\n"
    "EndFragment:__________\r\n" +
    markedHtml;

  const std::size_t startHtml = payload.find(markedHtml);
  const std::size_t endHtml = payload.size();
  const std::size_t startFragment = startHtml + startFragmentMarker.size();
  const std::size_t endFragment = endHtml - endFragmentMarker.size();

  replaceAll(payload, "StartHTML:__________", std::format("StartHTML:{:010}", startHtml));
  replaceAll(payload, "EndHTML:__________", std::format("EndHTML:{:010}", endHtml));
  replaceAll(payload, "StartFragment:__________", std::format("StartFragment:{:010}", startFragment));
  replaceAll(payload, "EndFragment:__________", std::format("EndFragment:{:010}", endFragment));
  return payload;
}

} // namespace clipboard
