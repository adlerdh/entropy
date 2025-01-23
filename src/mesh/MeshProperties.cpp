#include "mesh/MeshProperties.h"

MeshProperties::MeshProperties()
  : m_materialColor(1.0f, 1.0f, 1.0f)
{
}

const glm::vec3& MeshProperties::materialColor() const
{
  return m_materialColor;
}

void MeshProperties::setMaterialColor(const glm::vec3& color)
{
  m_materialColor = color;
}
