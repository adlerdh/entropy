#ifndef ISOSURFACE_H
#define ISOSURFACE_H

#include "logic/records/MeshRecord.h"

#include <glm/vec3.hpp>

#include <string>

/**
 * @brief Material properites for the Blinn-Phong reflection model
 */
struct SurfaceMaterial
{
  float ambient = 0.15f;  //!< Ambient light lighting contribution
  float diffuse = 0.75f;  //!< Diffuse reflection lighting contribution
  float specular = 0.10f; //!< Specular reflection lighting contribution
  float shininess = 8.0f; //!< Specular reflection coefficient
};

struct SurfaceQuality
{
  bool smoothNormals = true; //!< Do linear interpolation of normal vectors for lighting
};

/**
 * @brief Isosurface properties
 */
class Isosurface
{
public:
  // Isosurface() = default;
  // Isosurface(const Isosurface&) = delete;
  // Isosurface(Isosurface&&) = default;

  // ~Isosurface() = default;

  // Isosurface& operator=(const Isosurface&) = delete;
  // Isosurface& operator=(Isosurface&&) = default;

  std::string name;                  //!< Display name
  double value = 0.0;                //!< Isovalue, defined in image intensity units
  glm::vec3 color = glm::vec3{0.0f}; //!< RGB color
  SurfaceMaterial material;          //!< Material properties
  float opacity = 1.0f;              //!< Opacity
  bool visible = true;               //!< Visibility
  bool showIn2d = true;              //!< Show in 2D slice views
  float edgeStrength = 0.0f;         //!< Strength of edge outline, where 0.0f disables edges

  MeshRecord mesh;         //!< Mesh record of the isosurface
  bool meshInSync = false; //!< Is the mesh in sync with the isosurface value?

  glm::vec3 ambientColor() const { return this->material.ambient * this->color; }

  glm::vec3 diffuseColor() const { return this->material.diffuse * this->color; }

  glm::vec3 specularColor() const
  {
    static const glm::vec3 WHITE{1.0f};
    return this->material.specular * WHITE;
  }
};

#endif // ISOSURFACE_H
