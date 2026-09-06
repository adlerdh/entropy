#pragma once

#include "rendering/utility/gl/GLFrameBufferObject.h"
#include "rendering/utility/gl/GLTexture.h"
#include "rendering/utility/gl/GLVertexArrayObject.h"

#include <glm/vec2.hpp>

#include <array>
#include <optional>

namespace rendering::mesh
{

/**
 * @brief OpenGL textures and framebuffers needed by the mesh dual depth peeling pass
 *
 * The owner is intentionally limited to allocation and attachment. Render-pass code will decide draw buffers, clear
 * values, blend state, and shader execution.
 */
class MeshDdpResources
{
public:
  /**
   * @brief Construct an empty DDP resource owner
   */
  MeshDdpResources();

  MeshDdpResources(const MeshDdpResources&) = delete;
  MeshDdpResources& operator=(const MeshDdpResources&) = delete;

  MeshDdpResources(MeshDdpResources&&) = delete;
  MeshDdpResources& operator=(MeshDdpResources&&) = delete;

  /**
   * @brief Release framebuffer names and texture objects
   */
  ~MeshDdpResources();

  /**
   * @brief Allocate or resize all DDP attachments for a viewport
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
   * @brief FBO used for the dual depth peeling passes
   * @return Peeling framebuffer
   */
  GLFrameBufferObject& peelFbo() noexcept;

  /**
   * @brief FBO used for back-color accumulation
   * @return Back-blending framebuffer
   */
  GLFrameBufferObject& backBlendFbo() noexcept;

  /**
   * @brief Ping-pong min/max depth texture
   * @param index Ping-pong texture index, 0 or 1
   * @return Depth texture
   */
  GLTexture& depthTexture(std::size_t index);

  /**
   * @brief Ping-pong front-color accumulation texture
   * @param index Ping-pong texture index, 0 or 1
   * @return Front-color texture
   */
  GLTexture& frontColorTexture(std::size_t index);

  /**
   * @brief Ping-pong temporary back-color texture
   * @param index Ping-pong texture index, 0 or 1
   * @return Back temporary texture
   */
  GLTexture& backTempTexture(std::size_t index);

  /**
   * @brief Accumulated back-color texture
   * @return Back blender texture
   */
  GLTexture& backColorTexture();

  /**
   * @brief Empty VAO used for full-screen triangle shaders based on `gl_VertexID`
   * @return Full-screen triangle VAO
   */
  GLVertexArrayObject& fullScreenVao() noexcept;

private:
  static GLTexture makeAttachmentTexture();
  static void allocateDepthTexture(GLTexture& texture, const glm::uvec2& viewportSize);
  static void allocateColorTexture(GLTexture& texture, const glm::uvec2& viewportSize);
  void allocateTextures(const glm::uvec2& viewportSize);
  void attachFramebuffers();

  glm::uvec2 m_size{0u, 0u};
  bool m_initialized = false;
  std::array<std::optional<GLTexture>, 2> m_depthTextures;
  std::array<std::optional<GLTexture>, 2> m_frontColorTextures;
  std::array<std::optional<GLTexture>, 2> m_backTempTextures;
  std::optional<GLTexture> m_backColorTexture;
  GLVertexArrayObject m_fullScreenVao;
  GLFrameBufferObject m_peelFbo;
  GLFrameBufferObject m_backBlendFbo;
};

} // namespace rendering::mesh
