#ifndef IMAGE_HEADER_OVERRIDES
#define IMAGE_HEADER_OVERRIDES

#include "common/MathFuncs.h"

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

struct ImageHeaderOverrides
{
  ImageHeaderOverrides() = default;

  ImageHeaderOverrides(
    glm::uvec3 originalDimensions,
    glm::vec3 originalSpacing,
    glm::vec3 originalOrigin,
    glm::mat3 originalDirections
  )
    : m_originalDimensions(originalDimensions)
    , m_originalSpacing(originalSpacing)
    , m_originalOrigin(originalOrigin)
    , m_originalDirections(originalDirections)
    , m_closestOrthogonalDirections(math::computeClosestOrthogonalDirectionMatrix(originalDirections
      ))
  {
    m_originalIsOblique = math::computeSpiralCodeFromDirectionMatrix(m_originalDirections).second;
  }

  bool m_useIdentityPixelSpacings = false;   //!< Flag to use identity (1.0mm) pixel spacings
  bool m_useZeroPixelOrigin = false;         //!< Flag to use a zero pixel origin
  bool m_useIdentityPixelDirections = false; //!< Flag to use an identity direction matrix
  bool m_snapToClosestOrthogonalPixelDirections
    = false; //!< Flag to snap to the closest orthogonal direction matrix

  glm::uvec3 m_originalDimensions{0u};  //!< Original voxel dimensions
  glm::vec3 m_originalSpacing{1.0f};    //!< Original voxel spacing
  glm::vec3 m_originalOrigin{0.0f};     //!< Original voxel origin
  glm::mat3 m_originalDirections{1.0f}; //!< Original voxel direction cosines
  bool m_originalIsOblique = false;     //!< Is the original direction matrix oblique?

  /// Closest orthogonal directions to the original voxel direction cosines
  glm::mat3 m_closestOrthogonalDirections;
};

#endif // IMAGE_HEADER_OVERRIDES
