#include "slideio/SlideProperties.h"

#include <glm/glm.hpp>

namespace slideio
{

SlideProperties::SlideProperties()
  : m_displayName()
  , m_borderColor(0.0f, 0.5f, 1.0f)
  , m_visible(true)
  , m_opacity(1.0f)
  , m_intensityThresholds(0, 255)
  , m_edgesVisible(false)
  , m_edgesMagnitude(0.1f)
  , m_edgesSmoothing(1.0f)
  , m_annotVisible(true)
  , m_annotOpacity(1.0f)
{
}

const std::string& SlideProperties::displayName() const
{
  return m_displayName;
}

const glm::vec3& SlideProperties::borderColor() const
{
  return m_borderColor;
}

bool SlideProperties::visible() const
{
  return m_visible;
}

float SlideProperties::opacity() const
{
  return m_opacity;
}

bool SlideProperties::annotVisible() const
{
  return m_annotVisible;
}

float SlideProperties::annotOpacity() const
{
  return m_annotOpacity;
}

std::pair<uint8_t, uint8_t> SlideProperties::intensityThresholds() const
{
  return m_intensityThresholds;
}

bool SlideProperties::thresholdsActive() const
{
  return (0 < m_intensityThresholds.first || m_intensityThresholds.second < 255);
}

bool SlideProperties::edgesVisible() const
{
  return m_edgesVisible;
}

float SlideProperties::edgesMagnitude() const
{
  return m_edgesMagnitude;
}

float SlideProperties::edgesSmoothing() const
{
  return m_edgesSmoothing;
}

void SlideProperties::setDisplayName(std::string name)
{
  m_displayName = std::move(name);
}

void SlideProperties::setBorderColor(const glm::vec3& color)
{
  static const glm::vec3 sk_zero(0.0f);
  static const glm::vec3 sk_one(1.0f);

  m_borderColor = glm::clamp(color, sk_zero, sk_one);
}

void SlideProperties::setVisible(bool visible)
{
  m_visible = visible;
}

void SlideProperties::setOpacity(float opacity)
{
  m_opacity = glm::clamp(opacity, 0.0f, 1.0f);
}

void SlideProperties::setAnnotVisible(bool visible)
{
  m_annotVisible = visible;
}

void SlideProperties::setAnnotOpacity(float opacity)
{
  m_annotOpacity = opacity;
}

void SlideProperties::setIntensityThresholdLow(uint8_t low)
{
  if (low <= m_intensityThresholds.second)
  {
    m_intensityThresholds.first = low;
  }
}

void SlideProperties::setIntensityThresholdHigh(uint8_t high)
{
  if (m_intensityThresholds.first <= high)
  {
    m_intensityThresholds.second = high;
  }
}

void SlideProperties::setIntensityThresholds(const std::pair<uint8_t, uint8_t>& thresholds)
{
  if (thresholds.first <= thresholds.second)
  {
    m_intensityThresholds = thresholds;
  }
}

void SlideProperties::setEdgesVisible(bool visible)
{
  m_edgesVisible = visible;
}

void SlideProperties::setEdgesMagnitude(float mag)
{
  if (0.0f <= mag)
  {
    m_edgesMagnitude = mag;
  }
}

void SlideProperties::setEdgesSmoothing(float sigma)
{
  if (0.0f <= sigma)
  {
    m_edgesSmoothing = sigma;
  }
}

} // namespace slideio
