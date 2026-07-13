#pragma once

#include "rendering/utility/gl/GLFBOAttachmentTypes.h"
#include "rendering/utility/gl/GLTexture.h"
#include "rendering/utility/gl/GLTextureTypes.h"

#include <glad/glad.h>

#include <optional>
#include <string>

/**
 * @brief RAII wrapper around an OpenGL framebuffer object.
 *
 * The wrapper owns one framebuffer name and exposes the attachment operations currently needed by the renderer.
 * Attachment calls check framebuffer completeness after updating the target.
 */
class GLFrameBufferObject final
{
public:
  /// Create a named framebuffer wrapper for diagnostics.
  explicit GLFrameBufferObject(const std::string& name);

  GLFrameBufferObject(const GLFrameBufferObject&) = delete;
  GLFrameBufferObject& operator=(const GLFrameBufferObject&) = delete;

  GLFrameBufferObject(GLFrameBufferObject&&) noexcept;
  GLFrameBufferObject& operator=(GLFrameBufferObject&&) noexcept;

  /// Delete the framebuffer object if still owned.
  ~GLFrameBufferObject();

  /// Generate the framebuffer name if needed.
  void generate();

  /// Delete the framebuffer name and reset local state.
  void destroy();

  /// Bind the framebuffer to the requested draw, read, or draw/read target.
  void bind(const fbo::TargetType& target);

  /// Attach a 2D texture image to a framebuffer attachment point.
  void attach2DTexture(
    const fbo::TargetType& target,
    const fbo::AttachmentType& attachment,
    const GLTexture& tex,
    std::optional<int> colorAttachmentIndex = std::nullopt);

  /// Attach one cube-map face to a framebuffer attachment point.
  void attachCubeMapTexture(
    const fbo::TargetType& target,
    const fbo::AttachmentType& attachment,
    const GLTexture& tex,
    const tex::CubeMapFace& cubeMapFace,
    GLint level,
    std::optional<int> colorAttachmentIndex = std::nullopt);

  GLuint id() const;

private:
  void checkStatus();

  std::string m_name;
  GLuint m_id;
};
