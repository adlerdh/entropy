#pragma once

#include "common/MathFuncs.h"

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

/**
 * @brief User-selectable overrides applied to an image header's geometric metadata.
 *
 * The original geometry is retained so callers can toggle overrides without losing the values read
 * from disk. The default-constructed object represents "no overrides" with zero dimensions,
 * unit spacing, zero origin, and identity directions.
 */
struct ImageHeaderOverrides
{
  /// @brief Construct empty overrides with all override flags disabled.
  ImageHeaderOverrides() = default;

  /**
   * @brief Construct overrides from the original image geometry.
   * @param originalDims Original image dimensions in pixels.
   * @param originalSpacing Original pixel spacing in physical subject units.
   * @param originalOrigin Original image origin in physical subject space.
   * @param originalDirs Original pixel direction matrix.
   */
  ImageHeaderOverrides(
    glm::uvec3 originalDims,
    glm::vec3 originalSpacing,
    glm::vec3 originalOrigin,
    glm::mat3 originalDirs)
    : m_originalDims(originalDims)
    , m_originalSpacing(originalSpacing)
    , m_originalOrigin(originalOrigin)
    , m_originalDirs(originalDirs)
    , m_closestOrthogonalDirs(math::computeClosestOrthogonalDirectionMatrix(originalDirs))
  {
    m_originalIsOblique = math::computeSpiralCodeFromDirectionMatrix(m_originalDirs).second;
  }

  bool m_useIdentityPixelSpacings = false;               //!< Use unit pixel spacings instead of original spacing
  bool m_useZeroPixelOrigin = false;                     //!< Use a zero origin instead of original origin
  bool m_useIdentityPixelDirections = false;             //!< Use identity directions instead of original directions
  bool m_snapToClosestOrthogonalPixelDirections = false; //!< Use the closest orthogonal direction matrix

  glm::uvec3 m_originalDims{0u};     //!< Original voxel dimensions
  glm::vec3 m_originalSpacing{1.0f}; //!< Original voxel spacing
  glm::vec3 m_originalOrigin{0.0f};  //!< Original voxel origin
  glm::mat3 m_originalDirs{1.0f};    //!< Original voxel direction cosines
  bool m_originalIsOblique = false;  //!< Whether the original direction matrix is oblique

  /// @brief Closest orthogonal directions to the original voxel direction cosines.
  glm::mat3 m_closestOrthogonalDirs;
};
