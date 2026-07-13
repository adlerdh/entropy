#include "common/ClipboardFormats.h"

#include <catch2/catch_test_macros.hpp>

#include <charconv>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace
{

std::size_t parseOffset(const std::string& payload, std::string_view key)
{
  const std::size_t keyPos = payload.find(key);
  REQUIRE(keyPos != std::string::npos);
  const std::size_t valueBegin = keyPos + key.size();
  std::size_t value = 0;
  const auto* first = payload.data() + valueBegin;
  const auto* last = first + 10;
  const auto result = std::from_chars(first, last, value);
  REQUIRE(result.ec == std::errc{});
  return value;
}

} // namespace

TEST_CASE("Clipboard HTML escaping covers special characters", "[common][clipboard]")
{
  CHECK(
    clipboard::escapeHtmlText(R"(<tag attr="a&b">'x'</tag>)") ==
    "&lt;tag attr=&quot;a&amp;b&quot;&gt;&#39;x&#39;&lt;/tag&gt;");
}

TEST_CASE("Clipboard RGB colors format as CSS hex", "[common][clipboard]")
{
  CHECK(clipboard::rgbCssHex({0, 1, 15}) == "#00010F");
  CHECK(clipboard::rgbCssHex({127, 128, 255}) == "#7F80FF");
}

TEST_CASE("Clipboard RTF escaping covers control characters", "[common][clipboard]")
{
  CHECK(clipboard::escapeRtfText(R"(\{}\n)") == R"(\\\{\}\\n)");
  CHECK(clipboard::escapeRtfText("a\nb") == "a\\line\nb");
}

TEST_CASE("Clipboard RTF document contains color table and color runs", "[common][clipboard]")
{
  const std::vector<clipboard::ColoredTextRun> runs{
    {"AB", clipboard::Rgb8{255, 0, 0}},
    {" ", std::nullopt},
    {"C", clipboard::Rgb8{0, 128, 255}}};

  const std::string rtf = clipboard::rtfDocument(runs, 20);

  CHECK(rtf.starts_with("{\\rtf1\\ansi\\deff0"));
  CHECK(rtf.find("{\\fonttbl{\\f0\\fmodern Courier;}}") != std::string::npos);
  CHECK(rtf.find("{\\colortbl;\\red255\\green0\\blue0;\\red0\\green128\\blue255;}") != std::string::npos);
  CHECK(rtf.find("\\f0\\fs20 ") != std::string::npos);
  CHECK(rtf.find("\\cf1 AB\\cf0  \\cf2 C") != std::string::npos);
}

TEST_CASE("Clipboard RTF document can apply a background color", "[common][clipboard]")
{
  const std::vector<clipboard::ColoredTextRun> runs{{"A", clipboard::Rgb8{255, 255, 255}}};

  const std::string rtf = clipboard::rtfDocument(runs, 18, clipboard::Rgb8{16, 32, 48});

  CHECK(rtf.find("{\\colortbl;\\red16\\green32\\blue48;\\red255\\green255\\blue255;}") != std::string::npos);
  CHECK(rtf.find("\\f0\\fs18 \\highlight1 \\cf2 A") != std::string::npos);
}

TEST_CASE("Windows HTML clipboard payload has valid byte offsets", "[common][clipboard]")
{
  const std::string html = "<pre><span style=\"color:#FF0000\">A&amp;B</span></pre>";
  const std::string payload = clipboard::windowsHtmlClipboardPayload(html);

  const std::size_t startHtml = parseOffset(payload, "StartHTML:");
  const std::size_t endHtml = parseOffset(payload, "EndHTML:");
  const std::size_t startFragment = parseOffset(payload, "StartFragment:");
  const std::size_t endFragment = parseOffset(payload, "EndFragment:");

  REQUIRE(startHtml < startFragment);
  REQUIRE(startFragment < endFragment);
  REQUIRE(endFragment < endHtml);
  REQUIRE(endHtml == payload.size());

  CHECK(payload.substr(startFragment, endFragment - startFragment) == html);
  CHECK(payload.substr(startHtml, 20) == "<!--StartFragment-->");
  CHECK(payload.substr(endFragment, 18) == "<!--EndFragment-->");
}
