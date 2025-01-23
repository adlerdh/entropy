#ifndef SLIDE_TX_H
#define SLIDE_TX_H

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace slideio
{

/**
 * @brief Transformation of a slide relative to its Stack
 */
class SlideTransformation
{
public:
  /**
     * @brief Mode used to parameterize the shear transformation
     */
  enum class ShearParamMode
  {
    /// Parameterize shear using two separate shear angles for x and y (2 DOF)
    ShearAngles,

    /// Parameterize shear by applying a shear rotation prior to a scaling (1 DOF)
    ScaleRotation
  };

  explicit SlideTransformation();

  SlideTransformation(const SlideTransformation&) = default;
  SlideTransformation& operator=(const SlideTransformation&) = default;

  SlideTransformation(SlideTransformation&&) = default;
  SlideTransformation& operator=(SlideTransformation&&) = default;

  ~SlideTransformation() = default;

  /// Affine transformation from normalized Slide space to Slide Stack space.
  /// This transformation potentially includes scale and shear components.
  const glm::mat4& stack_O_slide(const glm::vec3& physicalSlideDims) const;

  /// Rigid-body transformation from normalized Slide space to Slide Stack space.
  const glm::mat4& stack_O_slide_rigid(const glm::vec3& physicalSlideDims) const;

  glm::vec2 normalizedTranslationXY() const;
  float stackTranslationZ() const;
  float rotationAngleZ() const;
  glm::vec2 shearAnglesXY() const;
  float scaleRotationAngle() const;
  glm::vec2 scaleFactorsXY() const;
  glm::vec2 normalizedRotationCenterXY() const;
  ShearParamMode shearParamMode() const;

  void setNormalizedTranslationXY(glm::vec2 translation);
  void setNormalizedTranslationX(float tx);
  void setNormalizedTranslationY(float ty);

  void setStackTranslationZ(float translation);

  void setAutoTranslateToTopOfStack(bool set);
  bool autoTranslateToTopOfStack() const;

  void setRotationAngleZ(float angleInDegrees);

  void setShearAnglesXY(glm::vec2 anglesInDegrees);
  void setShearAnglesX(float ax);
  void setShearAnglesY(float ay);

  void setScaleRotationAngle(float anglesInDegrees);

  void setScaleFactorsXY(glm::vec2 scaleFactors);
  void setScaleFactorsX(float sx);
  void setScaleFactorsY(float sy);

  void setNormalizedRotationCenterXY(glm::vec2 rotationCenter);
  void setNormalizedRotationCenterX(float cx);
  void setNormalizedRotationCenterY(float cy);

  void setShearParamMode(const ShearParamMode& mode);

  /**
     * @brief Reset all transformation parameters to identity.
     */
  void setIdentity();

private:
  void flagRecompute();

  /// Recompute the slide transformations
  void recompute(const glm::vec3& physicalSlideDims) const;

  glm::mat4 computeScaleAndShearTx() const;

  /// Transformation from slide in unit cube space to stack
  mutable glm::mat4 m_stack_O_slide;

  /// Rigid version of transformation from slide in unit cube space to stack,
  /// which ignores scaling and shearing.
  mutable glm::mat4 m_stack_O_slide_rigid;

  /// Flag to recompute the transformations
  mutable bool m_recomputeSlideToStackTx;

  /// Translation of slide (in normalized [0,1]^2 space)
  glm::vec2 m_normalizedTranslationAlongXY;

  /// Translation along z (in physical stack space)
  float m_stackTranslationAlongZ;

  /// Should the slide be automatically translated to the top of the slide stack upon loading?
  /// This is the default setting when no stackTranslationZ is provided in the JSON for the slide.
  bool m_autoTranslateToTopOfStack;

  /// Rotation angle of slide relative to stack Z axis in degrees. Constrained to [-180.0, 180.0].
  float m_rotationAngleZ_inDegrees;

  /// x,y shear angles in degrees. Constrained to [-90.0, 90.0].
  glm::vec2 m_shearAnglesAboutXY_inDegrees;

  /// Scale rotation angle in degrees. Constrained to [-180.0, 180.0].
  float m_scaleAngle_inDegrees;

  /// x,y scale factors, relative to 1.0 being identity
  glm::vec2 m_scaleFactorsAlongXY;

  /// x,y origin of scale, shear, and rotation (in normalized [0,1]^2 space)
  glm::vec2 m_normalizedRotationCenterAlongXY;

  ShearParamMode m_shearParamMode;

  /// Cached slide dimensions
  mutable glm::vec3 m_cachedPhysicalSlideDims;
};

} // namespace slideio

#endif // SLIDE_TX_H
