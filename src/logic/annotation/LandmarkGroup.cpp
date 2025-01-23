#include "logic/annotation/LandmarkGroup.h"

#include <limits>

namespace
{

static constexpr float sk_defaultOpacity = 0.75f;

} // namespace

LandmarkGroup::LandmarkGroup()
  : m_fileName()
  , m_name()
  , m_pointMap()
  , m_inVoxelSpace(false)
  , m_layer(0)
  , m_maxLayer(0)
  , m_visibility(true)
  , m_opacity(sk_defaultOpacity)
  , m_color{glm::vec3{1.0f, 0.0f, 0.0f}}
  , m_colorOverride(true)
  , m_textColor{glm::vec3{1.0f, 1.0f, 1.0f}}
  , m_renderLandmarkIndices(true)
  , m_renderLandmarkNames(false)
  , m_landmarkRadiusFactor(0.02f)
{
}

void LandmarkGroup::setFileName(const fs::path& fileName)
{
  m_fileName = fileName;
}

const fs::path& LandmarkGroup::getFileName() const
{
  return m_fileName;
}

void LandmarkGroup::setName(std::string name)
{
  m_name = std::move(name);
}

const std::string& LandmarkGroup::getName() const
{
  return m_name;
}

void LandmarkGroup::setPoints(std::map<size_t, PointRecord<LandmarkGroup::PositionType> > pointMap)
{
  m_pointMap = std::move(pointMap);
}

const std::map<size_t, PointRecord<LandmarkGroup::PositionType> >& LandmarkGroup::getPoints() const
{
  return m_pointMap;
}

void LandmarkGroup::setInVoxelSpace(bool inVoxelSpace)
{
  m_inVoxelSpace = inVoxelSpace;
}

bool LandmarkGroup::getInVoxelSpace() const
{
  return m_inVoxelSpace;
}

std::map<size_t, PointRecord<LandmarkGroup::PositionType> >& LandmarkGroup::getPoints()
{
  return m_pointMap;
}

size_t LandmarkGroup::addPoint(PointRecord<PositionType> point)
{
  size_t maxIndex = 0;

  for (const auto& p : m_pointMap)
  {
    if (p.first > maxIndex)
      maxIndex = p.first;
  }

  const size_t newIndex = maxIndex + 1;
  m_pointMap.emplace(newIndex, std::move(point));
  return newIndex;
}

void LandmarkGroup::addPoint(size_t index, PointRecord<PositionType> point)
{
  m_pointMap.emplace(index, point);
}

bool LandmarkGroup::removePoint(size_t index)
{
  return (m_pointMap.erase(index) > 0);
}

void LandmarkGroup::setLayer(uint32_t layer)
{
  m_layer = layer;
}

size_t LandmarkGroup::maxIndex() const
{
  size_t maxIndex = 0;

  for (const auto& p : m_pointMap)
  {
    if (p.first > maxIndex)
      maxIndex = p.first;
  }

  return maxIndex;
}

uint32_t LandmarkGroup::getLayer() const
{
  return m_layer;
}

void LandmarkGroup::setMaxLayer(uint32_t maxLayer)
{
  m_maxLayer = maxLayer;
}

uint32_t LandmarkGroup::getMaxLayer() const
{
  return m_maxLayer;
}

void LandmarkGroup::setVisibility(bool visibility)
{
  m_visibility = visibility;
}

bool LandmarkGroup::getVisibility() const
{
  return m_visibility;
}

void LandmarkGroup::setOpacity(float opacity)
{
  if (0.0f <= opacity && opacity <= 1.0f)
  {
    m_opacity = opacity;
  }
}

float LandmarkGroup::getOpacity() const
{
  return m_opacity;
}

void LandmarkGroup::setColor(glm::vec3 color)
{
  m_color = std::move(color);
}

const glm::vec3& LandmarkGroup::getColor() const
{
  return m_color;
}

void LandmarkGroup::setColorOverride(bool set)
{
  m_colorOverride = set;
}

bool LandmarkGroup::getColorOverride() const
{
  return m_colorOverride;
}

void LandmarkGroup::setTextColor(std::optional<glm::vec3> color)
{
  m_textColor = std::move(color);
}

const std::optional<glm::vec3>& LandmarkGroup::getTextColor() const
{
  return m_textColor;
}

void LandmarkGroup::setRenderLandmarkIndices(bool render)
{
  m_renderLandmarkIndices = render;
}

bool LandmarkGroup::getRenderLandmarkIndices() const
{
  return m_renderLandmarkIndices;
}

void LandmarkGroup::setRenderLandmarkNames(bool render)
{
  m_renderLandmarkNames = render;
}

bool LandmarkGroup::getRenderLandmarkNames() const
{
  return m_renderLandmarkNames;
}

void LandmarkGroup::setRadiusFactor(float factor)
{
  m_landmarkRadiusFactor = factor;
}

float LandmarkGroup::getRadiusFactor() const
{
  return m_landmarkRadiusFactor;
}
