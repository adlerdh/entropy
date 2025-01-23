#ifndef MESH_PROPERTIES_H
#define MESH_PROPERTIES_H

#include <glm/vec3.hpp>

class MeshProperties
{
public:
  MeshProperties();
  ~MeshProperties() = default;

  const glm::vec3& materialColor() const;
  void setMaterialColor(const glm::vec3& color);

private:
  /// Mesh color as RGBA, non-pre-multiplied by alpha
  glm::vec3 m_materialColor;
};

#endif // MESH_PROPERTIES_H
