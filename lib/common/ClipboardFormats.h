#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace clipboard
{

/**
 * @brief 8-bit RGB color used by formatted clipboard payloads.
 */
struct Rgb8
{
  uint8_t r = 0; //!< Red channel
  uint8_t g = 0; //!< Green channel
  uint8_t b = 0; //!< Blue channel

  friend bool operator==(const Rgb8&, const Rgb8&) = default;
};

/**
 * @brief Consecutive text with an optional foreground color.
 */
struct ColoredTextRun
{
  std::string text;          //!< Text in this run
  std::optional<Rgb8> color; //!< Optional foreground color
};

/**
 * @brief Format an RGB color as a CSS hex color string.
 * @param color Color to format.
 * @return String in "#RRGGBB" form.
 */
std::string rgbCssHex(Rgb8 color);

/**
 * @brief Escape one character for use in HTML text.
 * @param c Character to escape.
 * @return Escaped HTML text for the character.
 */
std::string escapeHtmlChar(char c);

/**
 * @brief Escape text for use in an HTML document body.
 * @param text Text to escape.
 * @return HTML-safe text.
 */
std::string escapeHtmlText(std::string_view text);

/**
 * @brief Escape text for use in an RTF document body.
 * @param text Text to escape.
 * @return RTF-safe text.
 */
std::string escapeRtfText(std::string_view text);

/**
 * @brief Create an RTF document containing colored text runs.
 * @param runs Text runs to write into the document.
 * @param fontSizeHalfPoints Font size in RTF half-points.
 * @param backgroundColor Optional document background color.
 * @return Complete RTF document.
 */
std::string rtfDocument(
  std::span<const ColoredTextRun> runs,
  int fontSizeHalfPoints = 18,
  std::optional<Rgb8> backgroundColor = std::nullopt);

/**
 * @brief Wrap an HTML fragment in the Windows HTML Clipboard Format.
 * @param html HTML fragment or document to publish.
 * @return Clipboard payload with byte-offset headers expected by Windows.
 */
std::string windowsHtmlClipboardPayload(std::string_view html);

} // namespace clipboard
