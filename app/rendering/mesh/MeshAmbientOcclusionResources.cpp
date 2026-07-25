#include "rendering/mesh/MeshAmbientOcclusionResources.h"

#include "common/Exception.hpp"

#include <glad/glad.h>
#include <glm/vec3.hpp>

namespace rendering::mesh
{

namespace
{

glm::uvec3 textureSize3D(const glm::uvec2& viewportSize) noexcept
{
  return glm::uvec3{viewportSize, 1u};
}

} // namespace

MeshAmbientOcclusionResources::MeshAmbientOcclusionResources()
  : m_geometryFbo("Mesh ambient occlusion geometry FBO"), m_occlusionFbo("Mesh ambient occlusion resolve FBO")
{
}

MeshAmbientOcclusionResources::~MeshAmbientOcclusionResources()
{
  clear();
}

bool MeshAmbientOcclusionResources::ensureSize(const glm::uvec2& viewportSize)
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

void MeshAmbientOcclusionResources::clear() noexcept
{
  m_fullScreenVao.destroy();
  m_normalTexture.reset();
  m_depthTexture.reset();
  m_occlusionTexture.reset();
  m_geometryFbo.destroy();
  m_occlusionFbo.destroy();
  m_size = glm::uvec2{0u, 0u};
  m_initialized = false;
}

bool MeshAmbientOcclusionResources::initialized() const noexcept
{
  return m_initialized;
}

glm::uvec2 MeshAmbientOcclusionResources::size() const noexcept
{
  return m_size;
}

GLFrameBufferObject& MeshAmbientOcclusionResources::geometryFbo() noexcept
{
  return m_geometryFbo;
}

GLFrameBufferObject& MeshAmbientOcclusionResources::occlusionFbo() noexcept
{
  return m_occlusionFbo;
}

GLTexture& MeshAmbientOcclusionResources::normalTexture()
{
  if (!m_normalTexture) {
    throwDebug("Mesh ambient occlusion normal texture is not allocated");
  }

  return *m_normalTexture;
}

GLTexture& MeshAmbientOcclusionResources::depthTexture()
{
  if (!m_depthTexture) {
    throwDebug("Mesh ambient occlusion depth texture is not allocated");
  }

  return *m_depthTexture;
}

GLTexture& MeshAmbientOcclusionResources::occlusionTexture()
{
  if (!m_occlusionTexture) {
    throwDebug("Mesh ambient occlusion texture is not allocated");
  }

  return *m_occlusionTexture;
}

GLVertexArrayObject& MeshAmbientOcclusionResources::fullScreenVao() noexcept
{
  return m_fullScreenVao;
}

GLTexture MeshAmbientOcclusionResources::makeNearestTexture()
{
  GLTexture texture(tex::Target::Texture2D);
  texture.generate();
  texture.setAutoGenerateMipmaps(false);
  texture.setMinificationFilter(tex::MinificationFilter::Nearest);
  texture.setMagnificationFilter(tex::MagnificationFilter::Nearest);
  texture.setWrapMode(tex::WrapMode::ClampToEdge);
  return texture;
}

GLTexture MeshAmbientOcclusionResources::makeLinearTexture()
{
  GLTexture texture(tex::Target::Texture2D);
  texture.generate();
  texture.setAutoGenerateMipmaps(false);
  texture.setMinificationFilter(tex::MinificationFilter::Linear);
  texture.setMagnificationFilter(tex::MagnificationFilter::Linear);
  texture.setWrapMode(tex::WrapMode::ClampToEdge);
  return texture;
}

void MeshAmbientOcclusionResources::allocateNormalTexture(GLTexture& texture, const glm::uvec2& viewportSize)
{
  texture.setSize(textureSize3D(viewportSize));
  texture.setData(
    0,
    tex::SizedInternalFormat::RGBA16F,
    tex::BufferPixelFormat::RGBA,
    tex::BufferPixelDataType::Float32,
    nullptr);
}

void MeshAmbientOcclusionResources::allocateDepthTexture(GLTexture& texture, const glm::uvec2& viewportSize)
{
  texture.setSize(textureSize3D(viewportSize));
  texture.setData(
    0,
    tex::SizedInternalFormat::Depth32F,
    tex::BufferPixelFormat::DepthComponent,
    tex::BufferPixelDataType::Float32,
    nullptr);
}

void MeshAmbientOcclusionResources::allocateOcclusionTexture(GLTexture& texture, const glm::uvec2& viewportSize)
{
  texture.setSize(textureSize3D(viewportSize));
  texture.setData(
    0,
    tex::SizedInternalFormat::R16F,
    tex::BufferPixelFormat::Red,
    tex::BufferPixelDataType::Float32,
    nullptr);
}

void MeshAmbientOcclusionResources::allocateTextures(const glm::uvec2& viewportSize)
{
  m_fullScreenVao.generate();
  m_normalTexture.emplace(makeNearestTexture());
  m_depthTexture.emplace(makeNearestTexture());
  m_occlusionTexture.emplace(makeLinearTexture());
  allocateNormalTexture(*m_normalTexture, viewportSize);
  allocateDepthTexture(*m_depthTexture, viewportSize);
  allocateOcclusionTexture(*m_occlusionTexture, viewportSize);
}

void MeshAmbientOcclusionResources::attachFramebuffers()
{
  if (!m_normalTexture || !m_depthTexture || !m_occlusionTexture) {
    throwDebug("Mesh ambient occlusion resources are incomplete");
  }

  m_geometryFbo.generate();
  m_geometryFbo.bind(fbo::TargetType::DrawAndRead);
  m_geometryFbo.attach2DTexture(fbo::TargetType::Draw, fbo::AttachmentType::Color, *m_normalTexture, 0);
  m_geometryFbo.attach2DTexture(fbo::TargetType::Draw, fbo::AttachmentType::Depth, *m_depthTexture);

  m_occlusionFbo.generate();
  m_occlusionFbo.bind(fbo::TargetType::DrawAndRead);
  m_occlusionFbo.attach2DTexture(fbo::TargetType::Draw, fbo::AttachmentType::Color, *m_occlusionTexture, 0);
}

} // namespace rendering::mesh
