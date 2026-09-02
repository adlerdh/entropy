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
  bool fillAboveIsovalue = false;    //!< Fill values above rather than below the isovalue
  bool visibleIn2d = true;           //!< Show contours on 2D views and 3D image planes
  bool visibleIn3d = true;           //!< Show raycasted and mesh surfaces in 3D views
  bool valueEditInProgress = false;  //!< Transient UI state used to defer mesh extraction during isovalue edits

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
