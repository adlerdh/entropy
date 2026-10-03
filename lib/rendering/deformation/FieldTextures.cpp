#include "rendering/deformation/FieldTextures.h"

#include "rendering/deformation/NumericalState.h"

#include <glad/glad.h>
#include <glm/vec3.hpp>

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace rendering::deformation
{
namespace
{
std::size_t checkedBytes(const ::deformation::FieldDomain& domain, std::size_t limit)
{
  static_assert(sizeof(glm::vec4) == 4 * sizeof(float));
  if (domain.sampleCount() > limit / sizeof(glm::vec4)) {
    throw std::invalid_argument("Field texture exceeds its byte budget");
  }
  return domain.sampleCount() * sizeof(glm::vec4);
}
GLenum target(const ::deformation::FieldDomain& domain)
{
  return domain.dimension() == ::deformation::SpatialDimension::Plane ? GL_TEXTURE_2D : GL_TEXTURE_3D;
}
} // namespace

FieldTexture::FieldTexture(const ::deformation::FieldDomain& domain, std::size_t maxBytes)
  : m_domain(domain), m_bytes(checkedBytes(domain, maxBytes)), m_texture(static_cast<tex::Target>(target(domain)))
{
  detail::NumericalState state;
  GLint limit = 0;
  glGetIntegerv(target(domain) == GL_TEXTURE_2D ? GL_MAX_TEXTURE_SIZE : GL_MAX_3D_TEXTURE_SIZE, &limit);
  if (limit <= 0 || std::ranges::any_of(domain.size(), [limit](auto n) { return n > static_cast<unsigned>(limit); })) {
    throw std::invalid_argument("Field dimensions exceed the current GL texture limit");
  }
  m_texture.generate();
  const auto& size = domain.size();
  m_texture.setSize({size[0], size[1], size[2]});
  const std::vector<glm::vec4> zero(domain.sampleCount(), glm::vec4(0.0f));
  m_texture.setData(
    0,
    tex::SizedInternalFormat::RGBA32F,
    tex::BufferPixelFormat::RGBA,
    tex::BufferPixelDataType::Float32,
    zero.data());
  m_texture.bind(0);
  const GLenum glTarget = target(domain);
  glTexParameteri(glTarget, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(glTarget, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(glTarget, GL_TEXTURE_BASE_LEVEL, 0);
  glTexParameteri(glTarget, GL_TEXTURE_MAX_LEVEL, 0);
  GLint width = 0;
  glGetTexLevelParameteriv(glTarget, 0, GL_TEXTURE_WIDTH, &width);
  if (width != static_cast<GLint>(size[0])) {
    throw std::runtime_error("Field texture allocation failed");
  }
}

const ::deformation::FieldDomain& FieldTexture::domain() const noexcept
{
  return m_domain;
}
std::size_t FieldTexture::bytes() const noexcept
{
  return m_bytes;
}
const GLTexture& FieldTexture::texture() const noexcept
{
  return m_texture;
}

void FieldTexture::upload(std::span<const glm::vec4> samples)
{
  if (samples.size() != m_domain.sampleCount()) {
    throw std::invalid_argument("Field upload has the wrong sample count");
  }
  detail::NumericalState state;
  m_texture.bind(0);
  const auto& size = m_domain.size();
  if (target(m_domain) == GL_TEXTURE_2D) {
    glTexSubImage2D(
      GL_TEXTURE_2D,
      0,
      0,
      0,
      static_cast<GLsizei>(size[0]),
      static_cast<GLsizei>(size[1]),
      GL_RGBA,
      GL_FLOAT,
      samples.data());
  }
  else {
    glTexSubImage3D(
      GL_TEXTURE_3D,
      0,
      0,
      0,
      0,
      static_cast<GLsizei>(size[0]),
      static_cast<GLsizei>(size[1]),
      static_cast<GLsizei>(size[2]),
      GL_RGBA,
      GL_FLOAT,
      samples.data());
  }
  if (glGetError() != GL_NO_ERROR) {
    throw std::runtime_error("Field upload failed");
  }
}

std::vector<glm::vec4> FieldTexture::readback() const
{
  std::vector<glm::vec4> samples(m_domain.sampleCount());
  detail::NumericalState state;
  m_texture.bind(0);
  glGetTexImage(target(m_domain), 0, GL_RGBA, GL_FLOAT, samples.data());
  if (glGetError() != GL_NO_ERROR) {
    throw std::runtime_error("Field readback failed");
  }
  return samples;
}
} // namespace rendering::deformation
