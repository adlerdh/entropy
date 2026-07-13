#pragma once

#include <glm/vec3.hpp>

#include <string>

/**
 * @brief Material coefficients for the Blinn-Phong reflection model.
 */
struct SurfaceMaterial
{
  float ambient = 0.15f;  //!< Ambient light lighting contribution
  float diffuse = 0.75f;  //!< Diffuse reflection lighting contribution
  float specular = 0.10f; //!< Specular reflection lighting contribution
  float shininess = 8.0f; //!< Specular reflection coefficient
};

/**
 * @brief Mesh display quality options for generated isosurfaces.
 */
struct SurfaceQuality
{
  bool smoothNormals = true; //!< Do linear interpolation of normal vectors for lighting
};

/**
 * @brief User-editable isosurface display and mesh synchronization state.
 */
class Isosurface
{
public:
  std::string name;                  //!< Display name
  double value = 0.0;                //!< Isovalue, defined in image intensity units
  glm::vec3 color = glm::vec3{0.0f}; //!< RGB color
  SurfaceMaterial material;          //!< Material properties
  float opacity = 1.0f;              //!< Surface/line opacity
  float fillOpacity = 0.0f;          //!< Fill opacity
  bool visible = true;               //!< Visibility
  bool showIn2d = true;              //!< Show in 2D slice views
  bool rimLightingEnabled = false;   //!< Enable view-angle rim opacity modulation and glow
  float rimOpacityStrength = 1.0f;   //!< View-angle opacity modulation strength
  float rimEmissionStrength = 1.0f;  //!< Additive view-angle rim-light strength
  float rimPower = 2.0f;             //!< Rim falloff exponent; higher values produce a narrower rim

  // MeshRecord mesh; //!< Mesh record of the isosurface
  bool meshInSync = false; //!< Is the mesh in sync with the isosurface value?

  /// @brief Get the ambient RGB contribution derived from material and surface color.
  glm::vec3 ambientColor() const
  {
    return this->material.ambient * this->color;
  }

  /// @brief Get the diffuse RGB contribution derived from material and surface color.
  glm::vec3 diffuseColor() const
  {
    return this->material.diffuse * this->color;
  }

  /// @brief Get the specular RGB contribution; specular highlights are white.
  glm::vec3 specularColor() const
  {
    return this->material.specular * glm::vec3{1.0f};
  }
};
