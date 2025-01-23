#include "logic_old/annotation/LandmarkGroupCpuRecord.h"

namespace
{

static constexpr float sk_defaultOpacity = 1.0f;

static const glm::vec3 sk_defaultColor{1.0f, 1.0f, 1.0f};

} // namespace

LandmarkGroupCpuRecord::LandmarkGroupCpuRecord()
  : m_name()
  , m_pointList()
  , m_layer(0)
  , m_maxLayer(0)
  , m_visibility(true)
  , m_opacity(sk_defaultOpacity)
  , m_color{sk_defaultColor}
{
}

void LandmarkGroupCpuRecord::setName(std::string name)
{
  m_name = std::move(name);
}

const std::string& LandmarkGroupCpuRecord::getName() const
{
  return m_name;
}

void LandmarkGroupCpuRecord::setPoints(PointList<PointRecord<PositionType> > pointList)
{
  m_pointList = std::move(pointList);
}

const PointList<PointRecord<LandmarkGroupCpuRecord::PositionType> >&
LandmarkGroupCpuRecord::getPoints() const
{
  return m_pointList;
}

void LandmarkGroupCpuRecord::setLayer(uint32_t layer)
{
  m_layer = layer;
}

uint32_t LandmarkGroupCpuRecord::getLayer() const
{
  return m_layer;
}

void LandmarkGroupCpuRecord::setMaxLayer(uint32_t maxLayer)
{
  m_maxLayer = maxLayer;
}

uint32_t LandmarkGroupCpuRecord::getMaxLayer() const
{
  return m_maxLayer;
}

void LandmarkGroupCpuRecord::setVisibility(bool visibility)
{
  m_visibility = visibility;
}

bool LandmarkGroupCpuRecord::getVisibility() const
{
  return m_visibility;
}

void LandmarkGroupCpuRecord::setOpacity(float opacity)
{
  if (0.0f <= opacity && opacity <= 1.0f)
  {
    m_opacity = opacity;
  }
}

float LandmarkGroupCpuRecord::getOpacity() const
{
  return m_opacity;
}

void LandmarkGroupCpuRecord::setColor(glm::vec3 color)
{
  m_color = std::move(color);
}

const glm::vec3& LandmarkGroupCpuRecord::getColor() const
{
  return m_color;
}
