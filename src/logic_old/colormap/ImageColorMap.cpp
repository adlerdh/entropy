#include "logic/colormap/ImageColorMap.h"

#include "common/HZeeException.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>

ImageColorMap::ImageColorMap(
  std::string name, std::string technicalName, std::string description, std::vector<glm::vec3> colors
)
  : m_name(std::move(name))
  , m_technicalName(std::move(technicalName))
  , m_description(std::move(description))
  , m_preview(0)
{
  if (colors.empty())
  {
    throw_debug("Empty color map");
  }

  for (const auto& x : colors)
  {
    m_colors_RGBA_F32.push_back(glm::vec4{x.r, x.g, x.b, 1.0f});
  }
}

ImageColorMap::ImageColorMap(
  std::string name, std::string technicalName, std::string description, std::vector<glm::vec4> colors
)
  : m_name(std::move(name))
  , m_technicalName(std::move(technicalName))
  , m_description(std::move(description))
  , m_colors_RGBA_F32(std::move(colors))
  , m_preview(0)
{
  if (m_colors_RGBA_F32.empty())
  {
    throw_debug("Empty color map");
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
  if (index >= m_colors_RGBA_F32.size())
  {
    std::ostringstream ss;
    ss << "Invalid color map index " << index << std::ends;
    throw_debug(ss.str());
  }

  return m_colors_RGBA_F32.at(index);
}

size_t ImageColorMap::numBytes_RGBA_F32() const
{
  return m_colors_RGBA_F32.size() * sizeof(glm::vec4);
}

const float* ImageColorMap::data_RGBA_F32() const
{
  return glm::value_ptr(m_colors_RGBA_F32[0]);
}

glm::vec2 ImageColorMap::slopeIntercept() const
{
  const float N = static_cast<float>(numColors());
  const float slope = (N - 1.0f) / N;
  const float intercept = 0.5f / N;
  return glm::vec2{slope, intercept};
}

void ImageColorMap::cyclicRotate(float fraction)
{
  const float f = (fraction < 0.0f) ? 1.0f - fraction : fraction;
  const int middle = static_cast<int>(f * m_colors_RGBA_F32.size());

  std::rotate(
    std::begin(m_colors_RGBA_F32),
    std::begin(m_colors_RGBA_F32) + middle,
    std::end(m_colors_RGBA_F32)
  );
}

void ImageColorMap::reverse()
{
  std::reverse(std::begin(m_colors_RGBA_F32), std::end(m_colors_RGBA_F32));
}

tex::SizedInternalFormat ImageColorMap::textureFormat_RGBA_F32()
{
  static const tex::SizedInternalFormat sk_format = tex::SizedInternalFormat::RGBA32F;
  return sk_format;
}
