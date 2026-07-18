#include "ui/LinuxUiScale.h"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>

namespace ui::linux_ui_scale
{
namespace
{
std::optional<std::string> environmentVariable(const char* name)
{
#ifdef _WIN32
  char* value = nullptr;
  std::size_t valueSize = 0;
  if (_dupenv_s(&value, &valueSize, name) != 0 || value == nullptr) {
    return std::nullopt;
  }

  std::string result{value, valueSize > 0 ? valueSize - 1 : 0};
  std::free(value);
  if (result.empty()) {
    return std::nullopt;
  }
  return result;
#else
  const char* value = std::getenv(name);
  if (value == nullptr || *value == '\0') {
    return std::nullopt;
  }
  return std::string{value};
#endif
}

std::optional<std::string_view> tagValue(std::string_view text, std::string_view tag)
{
  const std::string openTag = "<" + std::string{tag} + ">";
  const std::string closeTag = "</" + std::string{tag} + ">";
  const std::size_t begin = text.find(openTag);
  if (begin == std::string_view::npos) {
    return std::nullopt;
  }

  const std::size_t valueBegin = begin + openTag.size();
  const std::size_t end = text.find(closeTag, valueBegin);
  if (end == std::string_view::npos) {
    return std::nullopt;
  }

  return text.substr(valueBegin, end - valueBegin);
}

bool hasOnlyTrailingWhitespace(std::string_view text, std::size_t begin)
{
  for (std::size_t i = begin; i < text.size(); ++i) {
    if (!std::isspace(static_cast<unsigned char>(text[i]))) {
      return false;
    }
  }

  return true;
}

std::optional<float> parseScale(std::string_view scaleText)
{
  try {
    std::size_t parsedChars = 0;
    const float scale = std::stof(std::string{scaleText}, &parsedChars);
    if (
      parsedChars == 0 || !hasOnlyTrailingWhitespace(scaleText, parsedChars) || !std::isfinite(scale) || scale < 0.5f ||
      scale > 4.0f)
    {
      return std::nullopt;
    }
    return scale;
  }
  catch (...) {
    return std::nullopt;
  }
}

std::optional<float> scaleFromLogicalMonitorBlock(std::string_view block)
{
  const auto scaleText = tagValue(block, "scale");
  return scaleText ? parseScale(*scaleText) : std::nullopt;
}
} // namespace

std::optional<float> scaleFromMonitorsXml(std::string_view xml)
{
  std::optional<float> firstScale;
  std::size_t searchPos = 0;

  while (true) {
    const std::size_t begin = xml.find("<logicalmonitor>", searchPos);
    if (begin == std::string_view::npos) {
      break;
    }

    const std::size_t end = xml.find("</logicalmonitor>", begin);
    if (end == std::string_view::npos) {
      break;
    }

    const std::string_view block = xml.substr(begin, end - begin);
    const std::optional<float> scale = scaleFromLogicalMonitorBlock(block);
    if (scale && !firstScale) {
      firstScale = scale;
    }

    const auto primary = tagValue(block, "primary");
    if (scale && primary && *primary == "yes") {
      return scale;
    }

    searchPos = end + 1;
  }

  return firstScale;
}

std::optional<float> scaleFromMonitorsXmlFile(const std::filesystem::path& path)
{
  if (path.empty()) {
    return std::nullopt;
  }

  std::ifstream file(path);
  if (!file) {
    return std::nullopt;
  }

  const std::string text{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
  return scaleFromMonitorsXml(text);
}

std::filesystem::path defaultMonitorsXmlPath()
{
  if (const std::optional<std::string> configHome = environmentVariable("XDG_CONFIG_HOME")) {
    return std::filesystem::path{*configHome} / "monitors.xml";
  }

  if (const std::optional<std::string> home = environmentVariable("HOME")) {
    return std::filesystem::path{*home} / ".config" / "monitors.xml";
  }

  return {};
}

std::optional<float> primaryMonitorScale()
{
  return scaleFromMonitorsXmlFile(defaultMonitorsXmlPath());
}
} // namespace ui::linux_ui_scale
