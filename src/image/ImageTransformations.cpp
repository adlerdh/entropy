#include "image/ImageTransformations.h"

#include "common/MathFuncs.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/transform.hpp>

namespace
{

static const glm::mat4 sk_ident(1.0f);

}

ImageTransformations::ImageTransformations(
  const glm::uvec3& pixelDimensions,
  const glm::vec3& pixelSpacing,
  const glm::vec3& pixelOrigin,
  const glm::mat3& pixelDirections
)
  : m_headerOverrides(pixelDimensions, pixelSpacing, pixelOrigin, pixelDirections)
  ,

  m_is_worldDef_T_affine_locked(true)
  ,

  m_invPixelDimensions(math::computeInvPixelDimensions(pixelDimensions))
  ,

  m_worldDef_T_affine_TxType(ManualTransformationType::Similarity)
  ,

  m_subject_T_pixel(
    math::computeImagePixelToSubjectTransformation(pixelDirections, pixelSpacing, pixelOrigin)
  )
  , m_pixel_T_subject(glm::inverse(m_subject_T_pixel))
  ,

  m_texture_T_pixel(math::computeImagePixelToTextureTransformation(pixelDimensions))
  , m_pixel_T_texture(glm::inverse(m_texture_T_pixel))
  ,

  m_texture_T_subject(m_texture_T_pixel * m_pixel_T_subject)
  , m_subject_T_texture(glm::inverse(m_texture_T_subject))
  ,

  m_worldDef_T_affine_translation(0.0f)
  , m_worldDef_T_affine_rotation{1.0f, 0.0f, 0.0f, 0.0f}
  , m_worldDef_T_affine_scale(1.0f)
  ,

  m_worldDef_T_affine(1.0f)
  , m_enable_worldDef_T_affine(true)
  ,

  m_affine_T_subject(1.0f)
  , m_enable_affine_T_subject(true)
  , m_affine_T_subject_fileName(std::nullopt)
  ,

  m_worldDef_T_subject(1.0f)
  , m_subject_T_worldDef(1.0f)
  , m_subject_T_worldDef_invTransp(1.0f)
  ,

  m_worldDef_T_texture(1.0f)
  , m_texture_T_worldDef(1.0f)
  ,

  m_worldDef_T_pixel(1.0f)
  , m_pixel_T_worldDef(1.0f)
  , m_pixel_T_worldDef_invTransp(1.0f)
{
  initializeTransformations();
  updateTransformations();
}

void ImageTransformations::setHeaderOverrides(const ImageHeaderOverrides& overrides)
{
  m_headerOverrides = overrides;
  initializeTransformations();
  updateTransformations();
}

const ImageHeaderOverrides& ImageTransformations::getHeaderOverrides() const
{
  return m_headerOverrides;
}

bool ImageTransformations::is_worldDef_T_affine_locked() const
{
  return m_is_worldDef_T_affine_locked;
}

void ImageTransformations::set_worldDef_T_affine_locked(bool locked)
{
  m_is_worldDef_T_affine_locked = locked;
}

glm::vec3 ImageTransformations::invPixelDimensions() const
{
  return m_invPixelDimensions;
}

void ImageTransformations::set_worldDef_T_affine_translation(glm::vec3 worldDef_T_affine_translation)
{
  if (m_is_worldDef_T_affine_locked)
    return;
  m_worldDef_T_affine_translation = std::move(worldDef_T_affine_translation);
  updateTransformations();
}

const glm::vec3& ImageTransformations::get_worldDef_T_affine_translation() const
{
  return m_worldDef_T_affine_translation;
}

void ImageTransformations::set_worldDef_T_affine_rotation(glm::quat worldDef_T_affine_rotation)
{
  if (m_is_worldDef_T_affine_locked)
    return;
  m_worldDef_T_affine_rotation = std::move(worldDef_T_affine_rotation);
  updateTransformations();
}

const glm::quat& ImageTransformations::get_worldDef_T_affine_rotation() const
{
  return m_worldDef_T_affine_rotation;
}

void ImageTransformations::set_worldDef_T_affine_scale(glm::vec3 worldDef_T_affine_scale)
{
  if (m_is_worldDef_T_affine_locked)
    return;
  m_worldDef_T_affine_scale = std::move(worldDef_T_affine_scale);
  updateTransformations();
}

const glm::vec3& ImageTransformations::get_worldDef_T_affine_scale() const
{
  return m_worldDef_T_affine_scale;
}

const glm::mat4& ImageTransformations::get_worldDef_T_affine() const
{
  return (m_enable_worldDef_T_affine) ? m_worldDef_T_affine : sk_ident;
}

void ImageTransformations::reset_worldDef_T_affine()
{
  if (m_is_worldDef_T_affine_locked)
    return;
  m_worldDef_T_affine_translation = glm::vec3{0.0f};
  m_worldDef_T_affine_rotation = glm::quat{1.0f, 0.0f, 0.0f, 0.0f};
  m_worldDef_T_affine_scale = glm::vec3{1.0f};
  updateTransformations();
}

void ImageTransformations::set_enable_worldDef_T_affine(bool enable)
{
  m_enable_worldDef_T_affine = enable;
  updateTransformations();
}

bool ImageTransformations::get_enable_worldDef_T_affine() const
{
  return m_enable_worldDef_T_affine;
}

void ImageTransformations::set_enable_affine_T_subject(bool enable)
{
  m_enable_affine_T_subject = enable;
  updateTransformations();
}

bool ImageTransformations::get_enable_affine_T_subject() const
{
  return m_enable_affine_T_subject;
}

void ImageTransformations::set_affine_T_subject(glm::mat4 affine_T_subject)
{
  m_affine_T_subject = std::move(affine_T_subject);
  updateTransformations();
}

const glm::mat4& ImageTransformations::get_affine_T_subject() const
{
  return (m_enable_affine_T_subject) ? m_affine_T_subject : sk_ident;
}

void ImageTransformations::set_affine_T_subject_fileName(const std::optional<fs::path>& fileName)
{
  m_affine_T_subject_fileName = fileName;
}

const std::optional<fs::path>& ImageTransformations::get_affine_T_subject_fileName() const
{
  return m_affine_T_subject_fileName;
}

const glm::mat4& ImageTransformations::subject_T_pixel() const
{
  return m_subject_T_pixel;
}

const glm::mat4& ImageTransformations::pixel_T_subject() const
{
  return m_pixel_T_subject;
}

const glm::mat4& ImageTransformations::pixel_T_texture() const
{
  return m_pixel_T_texture;
}

const glm::mat4& ImageTransformations::texture_T_pixel() const
{
  return m_texture_T_pixel;
}

const glm::mat4& ImageTransformations::subject_T_texture() const
{
  return m_subject_T_texture;
}

const glm::mat4& ImageTransformations::texture_T_subject() const
{
  return m_texture_T_subject;
}

const glm::mat4& ImageTransformations::worldDef_T_subject() const
{
  return m_worldDef_T_subject;
}

const glm::mat4& ImageTransformations::subject_T_worldDef() const
{
  return m_subject_T_worldDef;
}

const glm::mat3& ImageTransformations::subject_T_worldDef_invTransp() const
{
  return m_subject_T_worldDef_invTransp;
}

const glm::mat4& ImageTransformations::worldDef_T_texture() const
{
  return m_worldDef_T_texture;
}

const glm::mat4& ImageTransformations::texture_T_worldDef() const
{
  return m_texture_T_worldDef;
}

const glm::mat4& ImageTransformations::worldDef_T_pixel() const
{
  return m_worldDef_T_pixel;
}

const glm::mat4& ImageTransformations::pixel_T_worldDef() const
{
  return m_pixel_T_worldDef;
}

const glm::mat3& ImageTransformations::pixel_T_worldDef_invTransp() const
{
  return m_pixel_T_worldDef_invTransp;
}

void ImageTransformations::initializeTransformations()
{
  const glm::vec3 spacing = m_headerOverrides.m_useIdentityPixelSpacings
                              ? glm::vec3(1.0f)
                              : m_headerOverrides.m_originalSpacing;

  const glm::vec3 origin = m_headerOverrides.m_useZeroPixelOrigin
                             ? glm::vec3(0.0f)
                             : m_headerOverrides.m_originalOrigin;

  glm::mat3 directions = m_headerOverrides.m_originalDirections;

  if (m_headerOverrides.m_useIdentityPixelDirections)
  {
    directions = glm::mat3{1.0f};
  }
  else if (m_headerOverrides.m_snapToClosestOrthogonalPixelDirections)
  {
    directions = m_headerOverrides.m_closestOrthogonalDirections;
  }

  m_subject_T_pixel = math::computeImagePixelToSubjectTransformation(directions, spacing, origin);

  m_pixel_T_subject = glm::inverse(m_subject_T_pixel);

  m_texture_T_pixel = math::computeImagePixelToTextureTransformation(
    m_headerOverrides.m_originalDimensions
  );

  m_pixel_T_texture = glm::inverse(m_texture_T_pixel);

  m_texture_T_subject = m_texture_T_pixel * m_pixel_T_subject;
  m_subject_T_texture = glm::inverse(m_texture_T_subject);
}

void ImageTransformations::updateTransformations()
{
  switch (m_worldDef_T_affine_TxType)
  {
  case ManualTransformationType::Rigid:
  {
    m_worldDef_T_affine = glm::translate(m_worldDef_T_affine_translation)
                          * glm::toMat4(m_worldDef_T_affine_rotation);
    break;
  }
  case ManualTransformationType::Similarity:
  {
    m_worldDef_T_affine = glm::translate(m_worldDef_T_affine_translation)
                          * glm::toMat4(m_worldDef_T_affine_rotation)
                          * glm::scale(m_worldDef_T_affine_scale);
    break;
  }
  }

  m_worldDef_T_subject = get_worldDef_T_affine() * get_affine_T_subject();
  m_subject_T_worldDef = glm::inverse(m_worldDef_T_subject);
  m_subject_T_worldDef_invTransp = glm::mat3{glm::inverseTranspose(m_subject_T_worldDef)};

  m_worldDef_T_texture = m_worldDef_T_subject * m_subject_T_texture;
  m_texture_T_worldDef = glm::inverse(m_worldDef_T_texture);

  m_worldDef_T_pixel = m_worldDef_T_subject * m_subject_T_pixel;
  m_pixel_T_worldDef = glm::inverse(m_worldDef_T_pixel);
  m_pixel_T_worldDef_invTransp = glm::mat3{glm::inverseTranspose(m_pixel_T_worldDef)};
}

std::ostream& operator<<(std::ostream& os, const ImageTransformations& tx)
{
  os << "pixel_T_texture: " << glm::to_string(tx.m_pixel_T_texture)
     << "\nsubject_T_pixel: " << glm::to_string(tx.m_subject_T_pixel)
     << "\naffine_T_subject: " << glm::to_string(tx.m_affine_T_subject)
     << "\nworldDef_T_affine: " << glm::to_string(tx.m_worldDef_T_affine)
     << "\n\ntexture_T_pixel: " << glm::to_string(tx.m_texture_T_pixel)
     << "\npixel_T_subject: " << glm::to_string(tx.m_pixel_T_subject)
     << "\nm_worldDef_T_texture: " << glm::to_string(tx.m_worldDef_T_texture)
     << "\nm_worldDef_T_pixel: " << glm::to_string(tx.m_worldDef_T_pixel);

  return os;
}
