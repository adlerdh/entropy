#include "logic_old/annotation/SlideAnnotationCpuRecord.h"
#include "logic_old/annotation/PlanarPolygon.h"

#include <glm/glm.hpp>

namespace
{

static constexpr float sk_defaultOpacity = 1.0f;

static const glm::vec3 sk_defaultColor{0.5f, 0.5f, 0.5f};

} // namespace

SlideAnnotationCpuRecord::SlideAnnotationCpuRecord()
  : m_polygon(nullptr)
  , m_layer(0)
  , m_maxLayer(0)
  , m_opacity(sk_defaultOpacity)
  , m_color{sk_defaultColor}
{
}

SlideAnnotationCpuRecord::SlideAnnotationCpuRecord(std::unique_ptr<PlanarPolygon> polygon)
  : m_polygon(std::move(polygon))
  , m_layer(0)
  , m_maxLayer(0)
  , m_opacity(sk_defaultOpacity)
  , m_color{sk_defaultColor}
{
}

void SlideAnnotationCpuRecord::setPolygon(std::unique_ptr<PlanarPolygon> polygon)
{
  m_polygon = std::move(polygon);
}

PlanarPolygon* SlideAnnotationCpuRecord::polygon()
{
  if (m_polygon)
  {
    return m_polygon.get();
  }
  return nullptr;
}

void SlideAnnotationCpuRecord::setLayer(uint32_t layer)
{
  m_layer = layer;
}

uint32_t SlideAnnotationCpuRecord::getLayer() const
{
  return m_layer;
}

void SlideAnnotationCpuRecord::setMaxLayer(uint32_t maxLayer)
{
  m_maxLayer = maxLayer;
}

uint32_t SlideAnnotationCpuRecord::getMaxLayer() const
{
  return m_maxLayer;
}

void SlideAnnotationCpuRecord::setOpacity(float opacity)
{
  if (0.0f <= opacity && opacity <= 1.0f)
  {
    m_opacity = opacity;
  }
}

float SlideAnnotationCpuRecord::getOpacity() const
{
  return m_opacity;
}

void SlideAnnotationCpuRecord::setColor(glm::vec3 color)
{
  m_color = std::move(color);
}

const glm::vec3& SlideAnnotationCpuRecord::getColor() const
{
  return m_color;
}
