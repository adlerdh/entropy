#pragma once

#include "rendering/utility/gl/GLFrameBufferObject.h"
#include "rendering/utility/gl/GLTexture.h"

#include <glm/vec2.hpp>

#include <optional>

namespace rendering::mesh
{

/**
 * @brief OpenGL resources for one directional-light mesh shadow map
 *
 * The resource owner only allocates the shadow-map depth texture and framebuffer. A render pass is responsible for
 * choosing the light matrix, viewport, clear state, and depth-only shader.
 */
class MeshShadowMapResources
{
public:
  /**
   * @brief Construct an empty shadow-map resource owner
   */
  MeshShadowMapResources();

  MeshShadowMapResources(const MeshShadowMapResources&) = delete;
  MeshShadowMapResources& operator=(const MeshShadowMapResources&) = delete;

  MeshShadowMapResources(MeshShadowMapResources&&) = delete;
  MeshShadowMapResources& operator=(MeshShadowMapResources&&) = delete;

  /**
   * @brief Release framebuffer and texture objects
   */
  ~MeshShadowMapResources();

  /**
   * @brief Allocate or resize shadow-map resources
   * @param sizePixels Square shadow-map width and height in pixels
   * @return True when resources were allocated or resized
   * @throw Propagates OpenGL allocation or framebuffer-completeness errors
   */
  bool ensureSize(uint32_t sizePixelsArg);

  /**
   * @brief Release all owned OpenGL objects
   */
  void clear() noexcept;

  /**
   * @brief Return whether a nonzero shadow map is allocated
   * @return Whether the resource set is ready for rendering
   */
  bool initialized() const noexcept;

  /**
   * @brief Return the allocated square shadow-map size
   * @return Shadow-map width and height in pixels
   */
  uint32_t sizePixels() const noexcept;

  /**
   * @brief Shadow-map framebuffer
   * @return Framebuffer with the depth texture attached
   */
  GLFrameBufferObject& fbo() noexcept;

  /**
   * @brief Shadow-map depth texture
   * @return Depth texture attached to the shadow-map framebuffer
   */
  GLTexture& depthTexture();

private:
  static GLTexture makeDepthTexture();
  static void allocateDepthTexture(GLTexture& texture, uint32_t sizePixels);
  void attachFramebuffer();

  uint32_t m_sizePixels = 0;
  bool m_initialized = false;
  std::optional<GLTexture> m_depthTexture;
  GLFrameBufferObject m_fbo;
};

} // namespace rendering::mesh
