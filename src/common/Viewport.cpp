#include "common/Viewport.h"

#include <glm/glm.hpp>

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <utility>

Viewport::Viewport(float left, float bottom, float width, float height)
  : m_left(left)
  , m_bottom(bottom)
  , m_width(width)
  , m_height(height)
  , m_devicePixelRatio(1.0f, 1.0f)
{
}

Viewport::Viewport(glm::vec4 viewportAsVec4)
  : Viewport(viewportAsVec4[0], viewportAsVec4[1], viewportAsVec4[2], viewportAsVec4[3])
{
}

Viewport::Viewport()
  : Viewport(0.0f, 0.0f, 1.0f, 1.0f)
{
}

void Viewport::setLeft(float l)
{
  m_left = l;
}

void Viewport::setBottom(float b)
{
  m_bottom = b;
}

void Viewport::setWidth(float w)
{
  m_width = w;
}

void Viewport::setHeight(float h)
{
  m_height = h;
}

void Viewport::setAsVec4(const glm::vec4& viewport)
{
  setLeft(viewport[0]);
  setBottom(viewport[1]);
  setWidth(viewport[2]);
  setHeight(viewport[3]);
}

float Viewport::left() const
{
  return m_left;
}

float Viewport::bottom() const
{
  return m_bottom;
}

float Viewport::width() const
{
  return m_width;
}

float Viewport::height() const
{
  return m_height;
}

float Viewport::area() const
{
  return width() * height();
}

glm::vec4 Viewport::getAsVec4() const
{
  return glm::vec4{m_left, m_bottom, m_width, m_height};
}

glm::vec4 Viewport::getDeviceAsVec4() const
{
  return glm::vec4{deviceLeft(), deviceBottom(), deviceWidth(), deviceHeight()};
}

float Viewport::deviceLeft() const
{
  return m_devicePixelRatio.x * m_left;
}

float Viewport::deviceBottom() const
{
  return m_devicePixelRatio.y * m_bottom;
}

float Viewport::deviceWidth() const
{
  return m_devicePixelRatio.x * m_width;
}

float Viewport::deviceHeight() const
{
  return m_devicePixelRatio.y * m_height;
}

float Viewport::deviceArea() const
{
  return deviceWidth() * deviceHeight();
}

float Viewport::aspectRatio() const
{
  return m_width / m_height;
}

float Viewport::deviceAspectRatio() const
{
  return deviceWidth() / deviceHeight();
}

void Viewport::setDevicePixelRatio(glm::vec2 ratio)
{
  spdlog::trace("Setting device pixel scale ratio to {}x{}", ratio.x, ratio.y);
  m_devicePixelRatio = std::move(ratio);
}

glm::vec2 Viewport::devicePixelRatio() const
{
  return m_devicePixelRatio;
}
