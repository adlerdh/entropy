#pragma once

#include <glm/vec3.hpp>

class MeshProperties
{
public:
  MeshProperties();
  ~MeshProperties() = default;

  const glm::vec3& materialColor() const;
  void setMaterialColor(const glm::vec3& color);

private:
  /// Mesh color as RGBA, non-premultiplied by alpha
  glm::vec3 m_materialColor;
};
