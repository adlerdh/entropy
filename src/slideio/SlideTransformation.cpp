#include "slideio/SlideTransformation.h"

#include "common/Exception.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/epsilon.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/transform.hpp>

#include <cmath>
#include <iostream>

namespace slideio
{

SlideTransformation::SlideTransformation()
  : m_stack_O_slide(1.0f)
  , m_stack_O_slide_rigid(1.0f)
  , m_recomputeSlideToStackTx(true)
  ,

  m_normalizedTranslationAlongXY(0.0f, 0.0f)
  , m_stackTranslationAlongZ(0.0f)
  , m_autoTranslateToTopOfStack(true)
  , m_rotationAngleZ_inDegrees(0.0f)
  , m_shearAnglesAboutXY_inDegrees(0.0f, 0.0f)
  , m_scaleAngle_inDegrees(0.0f)
  , m_scaleFactorsAlongXY(1.0f, 1.0f)
  ,

  /// @todo Make option to change rotation center to location of crosshairs provider:
  m_normalizedRotationCenterAlongXY(0.5f, 0.5f)
  ,

  m_shearParamMode(SlideTransformation::ShearParamMode::ShearAngles)
  , m_cachedPhysicalSlideDims(0.0f, 0.0f, 0.0f)
{
}

/// @todo GLM bug? is angle in degress?
const glm::mat4& SlideTransformation::stack_O_slide(const glm::vec3& physicalSlideDims) const
{
  if (physicalSlideDims != m_cachedPhysicalSlideDims)
  {
    m_cachedPhysicalSlideDims = physicalSlideDims;
    m_recomputeSlideToStackTx = true;
  }

  recompute(physicalSlideDims);
  return m_stack_O_slide;
}

const glm::mat4& SlideTransformation::stack_O_slide_rigid(const glm::vec3& physicalSlideDims) const
{
  if (physicalSlideDims != m_cachedPhysicalSlideDims)
  {
    m_cachedPhysicalSlideDims = physicalSlideDims;
    m_recomputeSlideToStackTx = true;
  }

  recompute(physicalSlideDims);
  return m_stack_O_slide_rigid;
}

void SlideTransformation::flagRecompute()
{
  m_recomputeSlideToStackTx = true;
}

void SlideTransformation::recompute(const glm::vec3& physicalSlideDims) const
{
  // Slides are stacked along the z axis of the Slide Stack
  static const glm::vec3 sk_zAxis(0.0f, 0.0f, 1.0f);

  if (!m_recomputeSlideToStackTx)
  {
    return;
  }

  // Translate, scale, shear, rotate the slide from original coordinates defined in
  // unit cube [0, 1]^3 space

  const glm::mat4 postScale =

    // Translate along stack Z axis:
    glm::translate(glm::vec3{0.0f, 0.0f, m_stackTranslationAlongZ}) *

    // Translate in X and Y:
    glm::translate(glm::vec3{
      m_normalizedTranslationAlongXY.x * physicalSlideDims.x,
      m_normalizedTranslationAlongXY.y * physicalSlideDims.y,
      0.0f
    })
    *

    // Translate back from center of rotation:
    glm::translate(glm::vec3{
      m_normalizedRotationCenterAlongXY.x * physicalSlideDims.x,
      m_normalizedRotationCenterAlongXY.y * physicalSlideDims.y,
      0.0f
    })
    *

    // Rotation about stack Z axis:
    glm::rotate(glm::radians(m_rotationAngleZ_inDegrees), sk_zAxis);

  m_stack_O_slide =

    postScale *

    // Additional scale and shear:
    computeScaleAndShearTx() *

    // Scale from unit cube space to physical World units:
    glm::scale(physicalSlideDims) *

    // Translate to center of rotation:
    glm::translate(glm::vec3{-m_normalizedRotationCenterAlongXY, 0.0f});

  m_stack_O_slide_rigid =

    postScale *

    // Translate to center of rotation:
    glm::translate(glm::vec3{-m_normalizedRotationCenterAlongXY, 0.0f});

  m_recomputeSlideToStackTx = false;
}

glm::vec2 SlideTransformation::normalizedTranslationXY() const
{
  return m_normalizedTranslationAlongXY;
}

float SlideTransformation::stackTranslationZ() const
{
  return m_stackTranslationAlongZ;
}

float SlideTransformation::rotationAngleZ() const
{
  return m_rotationAngleZ_inDegrees;
}

glm::vec2 SlideTransformation::shearAnglesXY() const
{
  return m_shearAnglesAboutXY_inDegrees;
}

float SlideTransformation::scaleRotationAngle() const
{
  return m_scaleAngle_inDegrees;
}

glm::vec2 SlideTransformation::scaleFactorsXY() const
{
  return m_scaleFactorsAlongXY;
}

glm::vec2 SlideTransformation::normalizedRotationCenterXY() const
{
  return m_normalizedRotationCenterAlongXY;
}

SlideTransformation::ShearParamMode SlideTransformation::shearParamMode() const
{
  return m_shearParamMode;
}

void SlideTransformation::setNormalizedTranslationXY(glm::vec2 vec)
{
  m_normalizedTranslationAlongXY = std::move(vec);
  flagRecompute();
}

void SlideTransformation::setNormalizedTranslationX(float tx)
{
  m_normalizedTranslationAlongXY.x = tx;
  flagRecompute();
}

void SlideTransformation::setNormalizedTranslationY(float ty)
{
  m_normalizedTranslationAlongXY.y = ty;
  flagRecompute();
}

void SlideTransformation::setStackTranslationZ(float t)
{
  m_stackTranslationAlongZ = t;
  flagRecompute();
}

void SlideTransformation::setAutoTranslateToTopOfStack(bool set)
{
  m_autoTranslateToTopOfStack = set;
}

bool SlideTransformation::autoTranslateToTopOfStack() const
{
  return m_autoTranslateToTopOfStack;
}

void SlideTransformation::setRotationAngleZ(float degrees)
{
  // Constrain to [-180, 180]
  m_rotationAngleZ_inDegrees = std::remainder(degrees, 360.0f);
  flagRecompute();
}

void SlideTransformation::setShearAnglesXY(glm::vec2 degrees)
{
  // Constrain to [-90, 90]
  m_shearAnglesAboutXY_inDegrees
    = glm::vec2{std::remainder(degrees.x, 180.0f), std::remainder(degrees.y, 180.0f)};

  flagRecompute();
}

void SlideTransformation::setShearAnglesX(float degrees)
{
  // Constrain to [-90, 90]
  m_shearAnglesAboutXY_inDegrees.x = std::remainder(degrees, 180.0f);
  flagRecompute();
}

void SlideTransformation::setShearAnglesY(float degrees)
{
  // Constrain to [-90, 90]
  m_shearAnglesAboutXY_inDegrees.y = std::remainder(degrees, 180.0f);
  flagRecompute();
}

void SlideTransformation::setScaleRotationAngle(float degrees)
{
  // Constrain to [-180, 180]
  m_scaleAngle_inDegrees = std::remainder(degrees, 360.0f);
  flagRecompute();
}

void SlideTransformation::setScaleFactorsXY(glm::vec2 scale)
{
  static const glm::vec2 sk_zero(0.0f);
  if (glm::any(glm::epsilonEqual(scale, sk_zero, glm::epsilon<float>())))
  {
    /// @todo Log
    std::cerr << "Invalid scale factor" << std::endl;
    return;
  }

  m_scaleFactorsAlongXY = std::move(scale);
  flagRecompute();
}

void SlideTransformation::setScaleFactorsX(float sx)
{
  if (glm::epsilonEqual(sx, 0.0f, glm::epsilon<float>()))
  {
    /// @todo Log
    std::cerr << "Invalid scale factor" << std::endl;
    return;
  }

  m_scaleFactorsAlongXY.x = sx;
  flagRecompute();
}

void SlideTransformation::setScaleFactorsY(float sy)
{
  if (glm::epsilonEqual(sy, 0.0f, glm::epsilon<float>()))
  {
    /// @todo Log
    std::cerr << "Invalid scale factor" << std::endl;
    return;
  }

  m_scaleFactorsAlongXY.y = sy;
  flagRecompute();
}

void SlideTransformation::setNormalizedRotationCenterXY(glm::vec2 origin)
{
  m_normalizedRotationCenterAlongXY = std::move(origin);
  flagRecompute();
}

void SlideTransformation::setNormalizedRotationCenterX(float cx)
{
  m_normalizedRotationCenterAlongXY.x = cx;
  flagRecompute();
}

void SlideTransformation::setNormalizedRotationCenterY(float cy)
{
  m_normalizedRotationCenterAlongXY.y = cy;
  flagRecompute();
}

void SlideTransformation::setShearParamMode(const ShearParamMode& mode)
{
  m_shearParamMode = mode;
  flagRecompute();
}

void SlideTransformation::setIdentity()
{
  m_normalizedTranslationAlongXY = {0.0f, 0.0f};
  m_stackTranslationAlongZ = 0.0f;
  m_rotationAngleZ_inDegrees = 0.0f;
  m_shearAnglesAboutXY_inDegrees = {0.0f, 0.0f};
  m_scaleAngle_inDegrees = 0.0f;
  m_scaleFactorsAlongXY = {1.0f, 1.0f};
  m_normalizedRotationCenterAlongXY = {0.5f, 0.5f};

  flagRecompute();
}

glm::mat4 SlideTransformation::computeScaleAndShearTx() const
{
  static const glm::vec3 sk_zAxis(0.0f, 0.0f, 1.0f);

  switch (m_shearParamMode)
  {
  case ShearParamMode::ScaleRotation:
  {
    return glm::rotate(glm::radians(-m_scaleAngle_inDegrees), sk_zAxis)
           * glm::scale(glm::vec3{m_scaleFactorsAlongXY, 1.0f})
           * glm::rotate(glm::radians(m_scaleAngle_inDegrees), sk_zAxis);
  }
  case ShearParamMode::ShearAngles:
  {
    glm::mat4 shearTx(1.0f);
    shearTx[0][1] = std::tan(glm::radians(m_shearAnglesAboutXY_inDegrees.x));
    shearTx[1][0] = std::tan(glm::radians(m_shearAnglesAboutXY_inDegrees.y));

    return shearTx * glm::scale(glm::vec3{m_scaleFactorsAlongXY, 1.0f});
  }
  }

  return glm::mat4{1.0f};
}

} // namespace slideio
