#pragma once

#include "common/MathFuncs.h"

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

struct ImageHeaderOverrides
{
  ImageHeaderOverrides() = default;

  ImageHeaderOverrides(glm::uvec3 originalDims, glm::vec3 originalSpacing,
                       glm::vec3 originalOrigin, glm::mat3 originalDirs)
    : m_originalDims(originalDims)
    , m_originalSpacing(originalSpacing)
    , m_originalOrigin(originalOrigin)
    , m_originalDirs(originalDirs)
    , m_closestOrthogonalDirs(math::computeClosestOrthogonalDirectionMatrix(originalDirs))
  {
    m_originalIsOblique = math::computeSpiralCodeFromDirectionMatrix(m_originalDirs).second;
  }

  bool m_useIdentityPixelSpacings = false;   //!< Flag to use identity (1.0mm) pixel spacings
  bool m_useZeroPixelOrigin = false;         //!< Flag to use a zero pixel origin
  bool m_useIdentityPixelDirections = false; //!< Flag to use an identity direction matrix
  bool m_snapToClosestOrthogonalPixelDirections = false; //!< Flag to snap to the closest orthogonal direction matrix

  glm::uvec3 m_originalDims{0u}; //!< Original voxel dimensions
  glm::vec3 m_originalSpacing{1.0f}; //!< Original voxel spacing
  glm::vec3 m_originalOrigin{0.0f}; //!< Original voxel origin
  glm::mat3 m_originalDirs{1.0f}; //!< Original voxel direction cosines
  bool m_originalIsOblique = false; //!< Is the original direction matrix oblique?

  /// Closest orthogonal directions to the original voxel direction cosines
  glm::mat3 m_closestOrthogonalDirs;
};
