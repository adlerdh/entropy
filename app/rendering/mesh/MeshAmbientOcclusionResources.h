#pragma once

#include "rendering/utility/gl/GLFrameBufferObject.h"
#include "rendering/utility/gl/GLTexture.h"
#include "rendering/utility/gl/GLVertexArrayObject.h"

#include <glm/vec2.hpp>

#include <optional>

namespace rendering::mesh
{

/**
 * @brief OpenGL resources for a screen-space mesh ambient occlusion pass
 *
 * The geometry pass stores depth plus encoded normals for the current 3D mesh view. The resolve pass writes one
 * single-channel occlusion factor texture that lit mesh shaders can sample in screen space.
 */
class MeshAmbientOcclusionResources
{
public:
  /**
   * @brief Construct an empty AO resource owner
   */
  MeshAmbientOcclusionResources();

  MeshAmbientOcclusionResources(const MeshAmbientOcclusionResources&) = delete;
  MeshAmbientOcclusionResources& operator=(const MeshAmbientOcclusionResources&) = delete;

  MeshAmbientOcclusionResources(MeshAmbientOcclusionResources&&) = delete;
  MeshAmbientOcclusionResources& operator=(MeshAmbientOcclusionResources&&) = delete;

  /**
   * @brief Release framebuffer and texture objects
   */
  ~MeshAmbientOcclusionResources();

  /**
   * @brief Allocate or resize AO resources for a viewport
   * @param viewportSize Viewport size in device pixels
   * @return True when resources were allocated or resized
   * @throw Propagates OpenGL allocation or framebuffer-completeness errors
   */
  bool ensureSize(const glm::uvec2& viewportSize);

  /**
   * @brief Release all owned OpenGL objects
   */
  void clear() noexcept;

  /**
   * @brief Return whether all resources are allocated for a nonzero viewport
   * @return Whether the resource set is ready for rendering
   */
  bool initialized() const noexcept;

  /**
   * @brief Return the allocated viewport size
   * @return Viewport size in device pixels
   */
  glm::uvec2 size() const noexcept;

  /**
   * @brief FBO used by the geometry pass
   * @return Geometry framebuffer
   */
  GLFrameBufferObject& geometryFbo() noexcept;

  /**
   * @brief FBO used by the AO resolve pass
   * @return AO resolve framebuffer
   */
  GLFrameBufferObject& occlusionFbo() noexcept;

  /**
   * @brief Encoded normal texture from the geometry pass
   * @return Normal texture
   */
  GLTexture& normalTexture();

  /**
   * @brief Depth texture from the geometry pass
   * @return Depth texture
   */
  GLTexture& depthTexture();

  /**
   * @brief Single-channel ambient occlusion factor texture
   * @return Ambient occlusion texture
   */
  GLTexture& occlusionTexture();

  /**
   * @brief Empty VAO for full-screen triangle shaders
   * @return Full-screen triangle VAO
   */
  GLVertexArrayObject& fullScreenVao() noexcept;

private:
  static GLTexture makeNearestTexture();
  static GLTexture makeLinearTexture();
  static void allocateNormalTexture(GLTexture& texture, const glm::uvec2& viewportSize);
  static void allocateDepthTexture(GLTexture& texture, const glm::uvec2& viewportSize);
  static void allocateOcclusionTexture(GLTexture& texture, const glm::uvec2& viewportSize);
  void allocateTextures(const glm::uvec2& viewportSize);
  void attachFramebuffers();

  glm::uvec2 m_size{0u, 0u};
  bool m_initialized = false;
  std::optional<GLTexture> m_normalTexture;
  std::optional<GLTexture> m_depthTexture;
  std::optional<GLTexture> m_occlusionTexture;
  GLVertexArrayObject m_fullScreenVao;
  GLFrameBufferObject m_geometryFbo;
  GLFrameBufferObject m_occlusionFbo;
};

} // namespace rendering::mesh
