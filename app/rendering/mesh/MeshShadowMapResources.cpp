#include "rendering/mesh/MeshShadowMapResources.h"

#include "common/Exception.hpp"

#include <glm/vec3.hpp>

#include <glad/glad.h>

namespace rendering::mesh
{

MeshShadowMapResources::MeshShadowMapResources() : m_fbo("Mesh shadow-map FBO") {}

MeshShadowMapResources::~MeshShadowMapResources()
{
  clear();
}

bool MeshShadowMapResources::ensureSize(const uint32_t sizePixelsArg)
{
  if (sizePixelsArg == 0u) {
    clear();
    return false;
  }

  if (m_initialized && m_sizePixels == sizePixelsArg) {
    return false;
  }

  clear();
  m_depthTexture.emplace(makeDepthTexture());
  allocateDepthTexture(*m_depthTexture, sizePixelsArg);
  attachFramebuffer();
  m_sizePixels = sizePixelsArg;
  m_initialized = true;
  return true;
}

void MeshShadowMapResources::clear() noexcept
{
  m_depthTexture.reset();
  m_fbo.destroy();
  m_sizePixels = 0;
  m_initialized = false;
}

bool MeshShadowMapResources::initialized() const noexcept
{
  return m_initialized;
}

uint32_t MeshShadowMapResources::sizePixels() const noexcept
{
  return m_sizePixels;
}

GLFrameBufferObject& MeshShadowMapResources::fbo() noexcept
{
  return m_fbo;
}

GLTexture& MeshShadowMapResources::depthTexture()
{
  if (!m_depthTexture) {
    throwDebug("Mesh shadow-map depth texture is not allocated");
  }

  return *m_depthTexture;
}

GLTexture MeshShadowMapResources::makeDepthTexture()
{
  GLTexture texture(tex::Target::Texture2D);
  texture.generate();
  texture.setAutoGenerateMipmaps(false);
  // The fragment shader performs explicit 3x3 percentage-closer filtering. Interpolating raw depth values before
  // those comparisons creates false intermediate blockers, so each tap must read one unfiltered depth texel.
  texture.setMinificationFilter(tex::MinificationFilter::Nearest);
  texture.setMagnificationFilter(tex::MagnificationFilter::Nearest);
  texture.setWrapMode(tex::WrapMode::ClampToBorder);
  texture.setBorderColor(glm::vec4{1.0f});
  return texture;
}

void MeshShadowMapResources::allocateDepthTexture(GLTexture& texture, const uint32_t sizePixels)
{
  texture.setSize(glm::uvec3{sizePixels, sizePixels, 1u});
  texture.setData(
    0,
    tex::SizedInternalFormat::Depth32F,
    tex::BufferPixelFormat::DepthComponent,
    tex::BufferPixelDataType::Float32,
    nullptr);
}

void MeshShadowMapResources::attachFramebuffer()
{
  if (!m_depthTexture) {
    throwDebug("Mesh shadow-map resources are incomplete");
  }

  m_fbo.generate();
  m_fbo.bind(fbo::TargetType::DrawAndRead);
  glDrawBuffer(GL_NONE);
  glReadBuffer(GL_NONE);
  m_fbo.attach2DTexture(fbo::TargetType::Draw, fbo::AttachmentType::Depth, *m_depthTexture);
}

} // namespace rendering::mesh
