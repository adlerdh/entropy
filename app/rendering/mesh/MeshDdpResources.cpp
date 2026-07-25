#include "rendering/mesh/MeshDdpResources.h"

#include "common/Exception.hpp"

#include <glm/vec3.hpp>

#include <cstddef>

namespace rendering::mesh
{

namespace
{

glm::uvec3 textureSize3D(const glm::uvec2& viewportSize) noexcept
{
  return glm::uvec3{viewportSize, 1u};
}

GLTexture& textureAt(std::array<std::optional<GLTexture>, 2>& textures, const std::size_t index)
{
  if (index >= textures.size() || !textures[index]) {
    throwDebug("Mesh DDP texture is not allocated");
  }

  return *textures[index];
}

} // namespace

MeshDdpResources::MeshDdpResources() : m_peelFbo("Mesh DDP peel FBO"), m_backBlendFbo("Mesh DDP back-blend FBO") {}

MeshDdpResources::~MeshDdpResources()
{
  clear();
}

bool MeshDdpResources::ensureSize(const glm::uvec2& viewportSize)
{
  if (viewportSize.x == 0u || viewportSize.y == 0u) {
    clear();
    return false;
  }

  if (m_initialized && m_size == viewportSize) {
    return false;
  }

  clear();
  allocateTextures(viewportSize);
  attachFramebuffers();
  m_size = viewportSize;
  m_initialized = true;
  return true;
}

void MeshDdpResources::clear() noexcept
{
  m_fullScreenVao.destroy();
  m_backColorTexture.reset();
  for (std::size_t i = 0; i < m_depthTextures.size(); ++i) {
    m_depthTextures[i].reset();
    m_frontColorTextures[i].reset();
    m_backTempTextures[i].reset();
  }

  m_peelFbo.destroy();
  m_backBlendFbo.destroy();
  m_size = glm::uvec2{0u, 0u};
  m_initialized = false;
}

bool MeshDdpResources::initialized() const noexcept
{
  return m_initialized;
}

glm::uvec2 MeshDdpResources::size() const noexcept
{
  return m_size;
}

GLFrameBufferObject& MeshDdpResources::peelFbo() noexcept
{
  return m_peelFbo;
}

GLFrameBufferObject& MeshDdpResources::backBlendFbo() noexcept
{
  return m_backBlendFbo;
}

GLTexture& MeshDdpResources::depthTexture(const std::size_t index)
{
  return textureAt(m_depthTextures, index);
}

GLTexture& MeshDdpResources::frontColorTexture(const std::size_t index)
{
  return textureAt(m_frontColorTextures, index);
}

GLTexture& MeshDdpResources::backTempTexture(const std::size_t index)
{
  return textureAt(m_backTempTextures, index);
}

GLTexture& MeshDdpResources::backColorTexture()
{
  if (!m_backColorTexture) {
    throwDebug("Mesh DDP back-color texture is not allocated");
  }

  return *m_backColorTexture;
}

GLVertexArrayObject& MeshDdpResources::fullScreenVao() noexcept
{
  return m_fullScreenVao;
}

GLTexture MeshDdpResources::makeAttachmentTexture()
{
  GLTexture texture(tex::Target::Texture2D);
  texture.generate();
  texture.setAutoGenerateMipmaps(false);
  texture.setMinificationFilter(tex::MinificationFilter::Nearest);
  texture.setMagnificationFilter(tex::MagnificationFilter::Nearest);
  texture.setWrapMode(tex::WrapMode::ClampToEdge);
  return texture;
}

void MeshDdpResources::allocateDepthTexture(GLTexture& texture, const glm::uvec2& viewportSize)
{
  texture.setSize(textureSize3D(viewportSize));
  texture.setData(
    0,
    tex::SizedInternalFormat::RG32F,
    tex::BufferPixelFormat::RG,
    tex::BufferPixelDataType::Float32,
    nullptr);
}

void MeshDdpResources::allocateColorTexture(GLTexture& texture, const glm::uvec2& viewportSize)
{
  texture.setSize(textureSize3D(viewportSize));
  texture.setData(
    0,
    tex::SizedInternalFormat::RGBA16F,
    tex::BufferPixelFormat::RGBA,
    tex::BufferPixelDataType::Float32,
    nullptr);
}

void MeshDdpResources::allocateTextures(const glm::uvec2& viewportSize)
{
  m_fullScreenVao.generate();

  for (std::size_t i = 0; i < m_depthTextures.size(); ++i) {
    m_depthTextures[i].emplace(makeAttachmentTexture());
    m_frontColorTextures[i].emplace(makeAttachmentTexture());
    m_backTempTextures[i].emplace(makeAttachmentTexture());

    allocateDepthTexture(*m_depthTextures[i], viewportSize);
    allocateColorTexture(*m_frontColorTextures[i], viewportSize);
    allocateColorTexture(*m_backTempTextures[i], viewportSize);
  }

  m_backColorTexture.emplace(makeAttachmentTexture());
  allocateColorTexture(*m_backColorTexture, viewportSize);
}

void MeshDdpResources::attachFramebuffers()
{
  if (!m_backColorTexture) {
    throwDebug("Mesh DDP resources are incomplete");
  }

  m_peelFbo.generate();
  m_peelFbo.bind(fbo::TargetType::DrawAndRead);
  m_peelFbo.attach2DTexture(fbo::TargetType::Draw, fbo::AttachmentType::Color, *m_depthTextures[0], 0);
  m_peelFbo.attach2DTexture(fbo::TargetType::Draw, fbo::AttachmentType::Color, *m_frontColorTextures[0], 1);
  m_peelFbo.attach2DTexture(fbo::TargetType::Draw, fbo::AttachmentType::Color, *m_backTempTextures[0], 2);
  m_peelFbo.attach2DTexture(fbo::TargetType::Draw, fbo::AttachmentType::Color, *m_depthTextures[1], 3);
  m_peelFbo.attach2DTexture(fbo::TargetType::Draw, fbo::AttachmentType::Color, *m_frontColorTextures[1], 4);
  m_peelFbo.attach2DTexture(fbo::TargetType::Draw, fbo::AttachmentType::Color, *m_backTempTextures[1], 5);
  m_peelFbo.attach2DTexture(fbo::TargetType::Draw, fbo::AttachmentType::Color, *m_backColorTexture, 6);

  m_backBlendFbo.generate();
  m_backBlendFbo.bind(fbo::TargetType::DrawAndRead);
  m_backBlendFbo.attach2DTexture(fbo::TargetType::Draw, fbo::AttachmentType::Color, *m_backColorTexture, 0);
}

} // namespace rendering::mesh
