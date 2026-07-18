#include "image/ImageColorMap.h"
#include "common/Exception.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <exception>
#include <set>
#include <utility>

namespace
{

std::vector<std::string> splitCsvFields(const std::string& line)
{
  std::vector<std::string> fields;
  std::string field;
  bool inQuotes = false;

  for (std::size_t i = 0; i < line.size(); ++i) {
    const char c = line[i];

    if (static_cast<char>(92) == c && i + 1 < line.size()) {
      field.push_back(line[++i]);
      continue;
    }

    if ('"' == c) {
      inQuotes = !inQuotes;
      continue;
    }

    if (',' == c && !inQuotes) {
      fields.push_back(std::move(field));
      field.clear();
      continue;
    }

    field.push_back(c);
  }

  fields.push_back(std::move(field));
  return fields;
}

} // namespace

ImageColorMap::ImageColorMap(
  std::string name,
  std::string technicalName,
  std::string description,
  InterpolationMode interpMode,
  const std::vector<glm::vec3>& colors)
  : m_name(std::move(name))
  , m_technicalName(std::move(technicalName))
  , m_description(std::move(description))
  , m_preview(0)
  , m_interpolationMode(interpMode)
  , m_transparentBorder(false)
{
  if (colors.empty()) {
    throw_debug("Empty color map")
  }

  for (const auto& x : colors) {
    m_colors_RGBA_F32.emplace_back(x.r, x.g, x.b, 1.0f);
  }
}

ImageColorMap::ImageColorMap(
  std::string name,
  std::string technicalName,
  std::string description,
  InterpolationMode interpMode,
  std::vector<glm::vec4> colors)
  : m_name(std::move(name))
  , m_technicalName(std::move(technicalName))
  , m_description(std::move(description))
  , m_colors_RGBA_F32(std::move(colors))
  , m_preview(0)
  , m_interpolationMode(interpMode)
  , m_transparentBorder(false)
{
  if (m_colors_RGBA_F32.empty()) {
    throw_debug("Empty color map")
  }
}

void ImageColorMap::setPreviewMap(std::vector<glm::vec4> preview)
{
  m_preview = std::move(preview);
}

bool ImageColorMap::hasPreviewMap() const
{
  return (!m_preview.empty());
}

size_t ImageColorMap::numPreviewMapColors() const
{
  return m_preview.size();
}

const float* ImageColorMap::getPreviewMap() const
{
  return glm::value_ptr(m_preview[0]);
}

const std::string& ImageColorMap::name() const
{
  return m_name;
}

const std::string& ImageColorMap::technicalName() const
{
  return m_technicalName;
}

const std::string& ImageColorMap::description() const
{
  return m_description;
}

size_t ImageColorMap::numColors() const
{
  return m_colors_RGBA_F32.size();
}

glm::vec4 ImageColorMap::color_RGBA_F32(size_t index) const
{
  if (index >= m_colors_RGBA_F32.size()) {
    std::ostringstream ss;
    ss << "Invalid color map index " << index << std::ends;
    throw_debug(ss.str())
  }

  return m_colors_RGBA_F32.at(index);
}

std::size_t ImageColorMap::numBytes_RGBA_F32() const
{
  return m_colors_RGBA_F32.size() * sizeof(glm::vec4);
}

const float* ImageColorMap::data_RGBA_F32() const
{
  return glm::value_ptr(m_colors_RGBA_F32[0]);
}

const std::vector<glm::vec4>& ImageColorMap::data_RGBA_asVector() const
{
  return m_colors_RGBA_F32;
}

bool ImageColorMap::setColorRGBA(std::size_t i, const glm::vec4& rgba)
{
  if (i < m_colors_RGBA_F32.size()) {
    m_colors_RGBA_F32[i] = rgba;
    return true;
  }

  spdlog::error("Could not set invalid index {} of colormap '{}'", i, name());
  return false;
}

glm::vec2 ImageColorMap::slopeIntercept(bool inverted) const
{
  const float slope = inverted ? -1.0f : 1.0f;
  const float intercept = inverted ? 1.0f : 0.0f;
  return glm::vec2{slope, intercept};
}

void ImageColorMap::setTransparentBorder(bool set)
{
  m_transparentBorder = set;
}

bool ImageColorMap::transparentBorder() const
{
  return m_transparentBorder;
}

void ImageColorMap::cyclicRotate(float fraction)
{
  float f = std::fmod(fraction, 1.0f);
  if (f < 0.0f) {
    f += 1.0f;
  }

  const auto middle = static_cast<std::size_t>(f * static_cast<float>(m_colors_RGBA_F32.size()));
  std::rotate(
    std::begin(m_colors_RGBA_F32),
    std::begin(m_colors_RGBA_F32) + static_cast<std::ptrdiff_t>(middle),
    std::end(m_colors_RGBA_F32));
}

void ImageColorMap::reverse()
{
  std::reverse(std::begin(m_colors_RGBA_F32), std::end(m_colors_RGBA_F32));
}

std::optional<ImageColorMap> ImageColorMap::loadImageColorMap(std::istringstream& csv)
{
  /// @todo Throws are not needed here. Just return flag that colormap could not be loaded.

  auto predicate = [](const auto& c) -> bool {
    static const std::set<char> allowedChars{' ', '-', '_', '(', ')'};
    const auto e = std::end(allowedChars);
    return !(std::isalnum(static_cast<unsigned char>(c)) || e != allowedChars.find(c));
  };

  // Read names and description from first three lines
  std::string briefName;     // line 1
  std::string technicalName; // line 2
  std::string description;   // line 3

  std::string line;

  if (std::getline(csv, line)) {
    briefName = line;
    briefName.erase(std::remove_if(std::begin(briefName), std::end(briefName), predicate), std::end(briefName));
  }
  else {
    spdlog::error("Could not extract brief name of colormap from CSV");
    return std::nullopt;
  }

  if (std::getline(csv, line)) {
    technicalName = line;
    technicalName.erase(
      std::remove_if(std::begin(technicalName), std::end(technicalName), predicate),
      std::end(technicalName));
  }
  else {
    spdlog::error("Could not extract technical name of colormap '{}'", briefName);
    return std::nullopt;
  }

  if (std::getline(csv, line)) {
    description = line;
    description.erase(std::remove_if(std::begin(description), std::end(description), predicate), std::end(description));
  }
  else {
    spdlog::error("Could not extract description of colormap '{}'", briefName);
    return std::nullopt;
  }

  // Read a color from each line of the file
  std::vector<glm::vec4> colors;
  size_t count = 0;

  while (getline(csv, line)) {
    try {
      const std::vector<std::string> c = splitCsvFields(line);

      if (3 == c.size()) {
        // Assume alpha component is 1:
        const float r = std::stof(c[0], nullptr);
        const float g = std::stof(c[1], nullptr);
        const float b = std::stof(c[2], nullptr);
        colors.emplace_back(r, g, b, 1.0f);
      }
      else if (4 == c.size()) {
        // Do NOT pre-multiply by the alpha component:
        const float r = std::stof(c[0], nullptr);
        const float g = std::stof(c[1], nullptr);
        const float b = std::stof(c[2], nullptr);
        const float a = std::stof(c[3], nullptr);
        colors.emplace_back(r, g, b, a);
      }
      else {
        spdlog::error("Invalid color map \"{}\": Color {} has {} components", briefName, count, c.size());
        return std::nullopt;
      }
    }
    catch (const std::exception& e) {
      spdlog::error("Invalid color map \"{}\": Could not parse color {}: {}", briefName, count, e.what());
      return std::nullopt;
    }

    ++count;
  }

  if (colors.empty()) {
    spdlog::error("Invalid color map '{}' has no colors", briefName);
    return std::nullopt;
  }

  SPDLOG_TRACE("Loaded image color map \"{}\" (\"{}\") with {} colors", briefName, technicalName, colors.size());

  return ImageColorMap(briefName, technicalName, description, InterpolationMode::Linear, std::move(colors));
}

ImageColorMap ImageColorMap::createLinearImageColorMap(
  const glm::vec4& startColor,
  const glm::vec4& endColor,
  std::size_t numSteps,
  const std::string& briefName,
  const std::string& description,
  const std::string& technicalName)
{
  const std::size_t N = (numSteps >= 2 ? numSteps : 2);

  // Number of pixels in preview image of the color map
  static constexpr int sk_previewSize = 64;

  // Linearly interpolate between start and end colors
  std::vector<glm::vec4> colors(N);

  const float Nm1 = static_cast<float>(N - 1);

  for (std::size_t i = 0; i < N; ++i) {
    colors[i] = startColor + static_cast<float>(i) / Nm1 * (endColor - startColor);
  }

  ImageColorMap map(briefName, technicalName, description, InterpolationMode::Linear, colors);

  std::vector<glm::vec4> previewColors(sk_previewSize);

  for (uint32_t i = 0; i < sk_previewSize; ++i) {
    previewColors[i] = glm::vec4{static_cast<float>(i) / sk_previewSize};
  }

  map.setPreviewMap(previewColors);

  return map;
}

void ImageColorMap::setInterpolationMode(const ImageColorMap::InterpolationMode& mode)
{
  m_interpolationMode = mode;
}

ImageColorMap::InterpolationMode ImageColorMap::interpolationMode() const
{
  return m_interpolationMode;
}
